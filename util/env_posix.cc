#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#ifndef __Fuchsia__
#include <sys/resource.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/env_posix_test_helper.h"
#include "util/mutexlock.h"
#include "util/posix_logger.h"

namespace leveldb {
namespace {
// Set by EnvPosixTestHelper::SetReadOnlyFDLimit() and MaxOpenFiles().
int g_open_read_only_file_limit = -1;
// Up to 1000 mmap regions for 64-bit binaries; none for 32-bit.
constexpr const int kDefaultMmapLimit = (sizeof(void*) >= 8) ? 1000 : 0;

// Can be set using EnvPosixTestHelper::SetReadOnlyMMapLimit().
int g_mmap_limit = kDefaultMmapLimit;

constexpr const int kOpenBaseFlags = 0;  // no use. in leveldb for O_CLOEXEC;

constexpr const size_t kWritableFileBufferSize = 65536;

Status PosixError(const std::string& context, int error_number) {
  if (error_number == ENONET) {
    return Status::NotFound(context, std::strerror(error_number));
  } else {
    return Status::IOError(context, std::strerror(error_number));
  }
}

// 防止耗尽资源所使用的辅助类
class Limiter {
 public:
  Limiter(int max_acquires)
#if !defined(NDEBUG)  // NDEBUG 控制宏是否禁用
      : max_acquires_(max_acquires),
#endif
        acquires_allowed_(max_acquires) {
    assert(max_acquires >= 0);
  }

  Limiter(const Limiter&) = delete;
  Limiter& operator=(const Limiter&) = delete;

  // 如果有可获取的资源，返回true
  bool Acquire() {
    int old_acquires_allowed =
        acquires_allowed_.fetch_sub(1, std::memory_order_relaxed);
    // old = ac
    // ac --
    // 注意返回值是修改前的值
    if (old_acquires_allowed > 0) {
      return true;  // 如果有资源 此处已经返回
    }
    int pre_increment_acquires_allowed =
        acquires_allowed_.fetch_add(1, std::memory_order_relaxed);
    //  避免编译警告
    (void)pre_increment_acquires_allowed;
    assert(pre_increment_acquires_allowed < max_acquires_);
    return false;
  }

  void Release() {
    int old_acquires_allowed =
        acquires_allowed_.fetch_add(1, std::memory_order_relaxed);
    (void)old_acquires_allowed;
    assert(old_acquires_allowed < max_acquires_);
  }

  ~Limiter() = default;

 private:
#if !defined(NDEBUG)
  // Catches an excessive number of Release() calls.
  const int max_acquires_;
#endif

  // 最多可获取的资源数
  //
  // This is a counter and is not tied to the invariants of any other class, so
  // it can be operated on safely using std::memory_order_relaxed.
  std::atomic<int> acquires_allowed_;
};

class PosixSequentialFile final : public SequentialFile {
 public:
  PosixSequentialFile(std::string filename, int fd)
      : fd_(fd),
        filename_(std::move(filename)) {
  }  // 移动filename所有权，接管filename的资源
  ~PosixSequentialFile() override { close(fd_); }

  Status Read(size_t n, Slice* result, char* scratch) override {
    Status status;
    while (true) {
      // signed
      ::ssize_t read_size = ::read(fd_, scratch, n);
      if (read_size < 0) {     // error
        if (errno == EINTR) {  // 读取被中断
          continue;
        }
        status = PosixError(filename_, errno);
        break;
      }
      return status;
    }
  }

  Status Skip(uint64_t n) {
    // lseek() 将文件fd_从SEEK_CUR位置往后设置n个字节
    if (::lseek(fd_, n, SEEK_CUR) == static_cast<off_t>(-1)) {
      return PosixError(filename_, errno);
    }
    return Status::OK();
  }

 private:
  const int fd_;
  const std::string filename_;
};

// 使用pread()实现的随机读取
// 线程安全
class PosixRandomAccessFile final : public RandomAccessFile {
 public:
  PosixRandomAccessFile(std::string filename, int fd, Limiter* fd_limiter)
      : has_permanent_fd_(fd_limiter->Acquire()),
        fd_(has_permanent_fd_ ? fd : -1),
        fd_limiter_(fd_limiter),
        filename_(std::move(filename)) {
    // fd_limiter->Acquire() 检查是否有可以获取的资源
    if (!has_permanent_fd_) {
      assert(fd_ == -1);
      ::close(fd);  // 注意关闭的是fd不是fd_
    }
  }

  ~PosixRandomAccessFile() override {
    if (has_permanent_fd_) {
      assert(fd_ != -1);
      ::close(fd_);
      fd_limiter_->Release();
    }
  }

  Status Read(uint64_t offset, size_t n, Slice* result,
              char* scratch) const override {
    int fd = fd_;
    if (!has_permanent_fd_) {
      fd = ::open(filename_.c_str(), O_RDONLY | kOpenBaseFlags);
      if (fd < 0) {
        return PosixError(filename_, errno);
      }
    }
    assert(fd != -1);
    Status status;
    ssize_t read_size = ::pread(fd, scratch, n, static_cast<off_t>(offset));
    // result 地址是传进来的 这里进行了拷贝
    *result = Slice(scratch, (read_size < 0) ? 0 : read_size);
    if (read_size < 0) {
      // An error: return a non-ok status.
      status = PosixError(filename_, errno);
    }
    if (!has_permanent_fd_) {
      assert(fd != fd_);  // 确保fd是有效的
      ::close(fd);
    }
    return status;
  }

 private:
  const bool
      has_permanent_fd_;  // 如果不是持续打开的fd_,那么每次读时重新打开文件
  const int fd_;  // 当 has_permanent_fd_ = false  -> fd = -1
  Limiter* const fd_limiter_;
  const std::string filename_;
};

// 使用mmap()实现的随机读取
// 线程安全
class PosixMmapReadableFile final : public RandomAccessFile {
 public:
  // mmap_base[0, length - 1] 指向 mmap 映射的文件，该实例会接管这块区域
  PosixMmapReadableFile(std::string filename, char* mmap_base, size_t length,
                        Limiter* mmap_limiter)
      : mmap_base_(mmap_base),
        length_(length),
        mmap_limiter_(mmap_limiter),
        filename_(std::move(filename)) {}

  ~PosixMmapReadableFile() override {
    ::munmap(static_cast<void*>(mmap_base_), length_);
    mmap_limiter_->Release();
  }

  Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) {
    if (offset + n > length_) {
      *result = Slice();
      return PosixError(filename_, EINVAL);
    }
    *result = Slice(mmap_base_ + offset, n);
    return Status::OK();
  }

 private:
  char* const mmap_base_;
  const size_t length_;
  Limiter* const mmap_limiter_;
  const std::string filename_;
};

class PosixWritableFile final : public WritableFile {
 public:
  PosixWritableFile(std::string filename, int fd)
      : pos_(0),
        fd_(fd),
        is_mainfest_(IsManifest(filename)),
        filename_(std::move(filename)),
        dirname_(Dirname(filename_)) {}

  ~PosixWritableFile() override {
    if (fd_ >= 0) {
      // 忽略可能的错误
      Close();
    }
  }

  Status Append(const Slice& data) override {
    size_t write_size = data.size();
    const char* write_data = data.data();

    // buffer装得下
    size_t copy_size = std::min(write_size, kWritableFileBufferSize - pos_);
    std::memcpy(buf_ + pos_, write_data, copy_size);
    write_data += copy_size;
    write_size -= copy_size;
    pos_ += copy_size;
    if (write_size == 0) {
      return Status::OK();
    }

    // 装不下
    Status status = FlushBuffer();
    if (!status.ok()) {
      return status;
    }

    // 小的写入buffer，大的直接写
    if (write_size < kWritableFileBufferSize) {
      std::memcpy(buf_, write_data, write_size);
      pos_ = write_size;
      return Status::OK();
    }

    return WriteUnBuffered(write_data, write_size);
  }

  Status Close() override {
    Status status = FlushBuffer();
    const int close_result = ::close(fd_);
    if (close_result < 0 && status.ok()) {
      status = PosixError(filename_, errno);
    }
    fd_ = -1;
    return status;
  }

  Status Flush() override { return FlushBuffer(); }

  Status Sync() override {
    Status status = SyncDirIfManifest();
    if (!status.ok()) {
      return status;
    }
    status = FlushBuffer();
    if (!status.ok()) {
      return status;
    }
    return SyncFd(fd_, filename_);
  }

 private:
  Status FlushBuffer() {
    Status status = WriteUnBuffered(buf_, pos_);
    pos_ = 0;
    return status;
  }

  Status WriteUnBuffered(const char* data, size_t size) {
    while (size > 0) {
      ssize_t write_result = ::write(fd_, data, size);
      if (write_result < 0) {
        if (errno == EINTR) {
          continue;  // retry
        }
        return PosixError(filename_, errno);
      }
      data += write_result;
      size -= write_result;
    }
    return Status::OK();
  }

  Status SyncDirIfManifest() {
    Status status;
    if (!is_mainfest_) {
      return status;
    }
    int fd = ::open(dirname_.c_str(), O_RDONLY | kOpenBaseFlags);
    if (fd < 0) {
      status = PosixError(dirname_, errno);
    } else {
      status = SyncFd(fd, dirname_);
      ::close(fd);
    }
    return status;
  }

  // 将数据写入磁盘，fd_path只用于描述信息
  static Status SyncFd(int fd, const std::string& fd_path) {
    bool sync_success = ::fsync(fd) == 0;
    if (sync_success) {
      return Status::OK();
    }
    return PosixError(fd_path, errno);
  }

  // 返回文件路径中所包含的dir路径
  // 如果不存在路径，返回"."
  static std::string Dirname(const std::string& filename) {
    std::string::size_type separator_pos = filename.rfind('/');
    // 无 '/'
    if (separator_pos == std::string::npos) {
      return std::string(".");
    }

    // 确保文件名中没有'/'
    assert(filename.find('/', separator_pos + 1) == std::string::npos);

    return filename.substr(0, separator_pos);
  }

  // */*/filename -> filename
  // 结果依赖于filename
  static Slice Basename(const std::string& filename) {
    std::string::size_type separator_pos = filename.rfind('/');
    if (separator_pos == std::string::npos) {
      return Slice(filename);
    }
    // 确保文件名中没有'/'
    assert(filename.find('/', separator_pos + 1) == std::string::npos);

    return Slice(filename.data() + separator_pos + 1,
                 filename.length() - separator_pos - 1);
  }

  static bool IsManifest(const std::string& filename) {
    return Basename(filename).starts_with("MANIFEST");
  }

  char buf_[kWritableFileBufferSize];  // buf_ 写缓冲区
  size_t pos_;
  int fd_;

  const bool is_mainfest_;  // True if the file's name starts with MANIFEST.
  const std::string filename_;
  const std::string dirname_;  // The directory of filename_.
};

// TODO LockOrUnLock
int LockOrUnlock(int fd, bool lock) {
  errno = 0;
  struct ::flock file_lock_info;
  std::memset(&file_lock_info, 0, sizeof(file_lock_info));
  file_lock_info.l_type = (lock ? F_WRLCK : F_UNLCK);
  file_lock_info.l_whence = SEEK_SET;
  file_lock_info.l_start = 0;
  file_lock_info.l_len = 0;  // for entire file
  return ::fcntl(fd, F_SETLK, &file_lock_info);
}

class PosixFileLock : public FileLock {
 public:
  PosixFileLock(int fd, std::string filename)
      : fd_(fd), filename_(std::move(filename)) {}
  int fd() const { return fd_; }
  const std::string& filename() const { return filename_; }

 private:
  const int fd_;
  const std::string filename_;
};

// 跟踪由PosixEnv::LockFile()上锁的文件
class PosixLockTable {
 public:
  bool Insert(const std::string& fname) LOCKS_EXCLUDED(mu_) {
    MutexLock l(&mu_);
    bool succeeded = locked_files_.insert(fname).second;
    return succeeded;
  }

  void Remove(const std::string& fname) LOCKS_EXCLUDED(mu_) {
    MutexLock l(&mu_);
    locked_files_.erase(fname);
  }

 private:
  port::Mutex mu_;
  std::set<std::string> locked_files_ GUARDED_BY(mu_);
};

class PosixEnv : public Env {
 public:
 private:
  void BackgroundThreadMain();

  static void BackgroundThreadEntryPoint(PosixEnv* env) {
    env->BackgroundThreadMain();
  }

  struct BackgroundWorkItem {
    explicit BackgroundWorkItem(void (*function)(void* arg), void* arg)
        : function(function), arg(arg) {}

    void (*const function)(void*);
    void* const arg;
  };

  port::Mutex background_work_mutex_;
  port::CondVar background_work_cv_ GUARDED_BY(background_work_mutex_);
  bool started_background_thread_ GUARDED_BY(background_work_mutex_);

  std::queue<BackgroundWorkItem> background_work_queue_
      GUARDED_BY(background_work_mutex_);
  PosixLockTable locks_;
  Limiter mmap_limiter_;
  Limiter fd_limiter;
};

int MaxMmaps() { return g_mmap_limit; }
int MaxOpenFiles() {
  if (g_open_read_only_file_limit >= 0) {
    return g_open_read_only_file_limit;
  }
  struct ::rlimit rlim;
  if (::getrlimit(RLIMIT_NOFILE, &rlim)) {
    // getrlimit failed, fallback to hard-coded default.
    g_open_read_only_file_limit = 50;
  } else if (rlim.rlim_cur == RLIM_INFINITY) {
    g_open_read_only_file_limit = std::numeric_limits<int>::max();
  } else {
    // 20 %
    g_open_read_only_file_limit = rlim.rlim_cur / 5;
  }
  return g_open_read_only_file_limit;
}

//  TODO in leveldb these out of no name
void BackgroundThreadMain() {}
}  // namespace
}  // namespace leveldb
