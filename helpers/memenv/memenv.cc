#include "helpers/memenv/memenv.h"

#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "leveldb/env.h"
#include "leveldb/status.h"
#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/mutexlock.h"

namespace leveldb {
namespace {
class FileState {
 public:
  FileState() : refs_(0), size_(0) {}
  FileState(const FileState&) = delete;
  FileState& operator=(const FileState&) = delete;

  void Ref() {
    // MutexLock 析构时自动解锁
    MutexLock lock(&refs_mutex_);
    ++refs_;
  }

  // 如果解除最后一个引用则析构
  void Unref() {
    bool do_delete = false;
    {
      MutexLock lock(&refs_mutex_);
      --refs_;
      assert(refs_ >= 0);
      if (refs_ <= 0) {
        do_delete = true;
      }
    }

    if (do_delete) {
      delete this;
    }
  }

  // 不清楚到底如何界定函数是否足够简单
  // 可能因为存在锁操作
  // 这里没有采用小写的函数命名法

  uint64_t Size() const {
    MutexLock lock(&blocks_mutex_);
    return size_;
  }

  void Truncate() {
    MutexLock lock(&blocks_mutex_);
    for (char*& block : blocks_) {
      delete[] block;
    }
    blocks_.clear();
    size_ = 0;
  }

  // 模拟文件读
  Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const {
    MutexLock lock(&blocks_mutex_);
    if (offset > size_) {
      return Status::IOError("Offset greater than file size.");
    }
    const uint64_t available = size_ - offset;
    if (n > available) {
      n = static_cast<size_t>(available);
    }
    if (n == 0) {
      *result = Slice();
      return Status::OK();
    }
    // 视平台而言 uint64_t 的 offset 可能长于 size_t
    assert(offset / kBlockSize <= std::numeric_limits<size_t>::max());
    size_t block = static_cast<size_t>(offset / kBlockSize);
    size_t block_offset = offset % kBlockSize;
    size_t bytes_to_copy = n;
    char* dst = scratch;

    while (bytes_to_copy > 0) {
      size_t avail = std::min(kBlockSize - block_offset, bytes_to_copy);
      std::memcpy(dst, blocks_[block] + block_offset, avail);
      bytes_to_copy -= avail;
      dst += avail;
      block++;
      block_offset = 0;
    }
    *result = Slice(scratch, n);
    return Status::OK();
  }

  Status Append(const Slice& data) {
    const char* src = data.data();
    size_t src_len = data.size();
    MutexLock lock(&blocks_mutex_);
    while (src_len > 0) {
      size_t avail;
      size_t offset = size_ % kBlockSize;
      if (offset != 0) {
        // 最后一块的剩余空间
        avail = kBlockSize - offset;
      } else {
        blocks_.push_back(new char[kBlockSize]);
        avail = kBlockSize;
      }

      if (avail > src_len) {
        avail = src_len;
      }
      // back() 返回最后一个元素的引用
      std::memcpy(blocks_.back() + offset, src, avail);
      src_len -= avail;
      src += avail;
      size_ += avail;
    }
    return Status::OK();
  }

 private:
  enum { kBlockSize = 8 * 1024 };
  // TODO 私有析构 只应该使用Unref()
  ~FileState() { Truncate(); }
  port::Mutex refs_mutex_;
  int refs_ GUARDED_BY(refs_mutex_);
  mutable port::Mutex
      blocks_mutex_;  // mutable 允许在const成员函数中修改被修饰的成员变量
  std::vector<char*> blocks_ GUARDED_BY(blocks_mutex_);
  uint64_t size_ GUARDED_BY(blocks_mutex_);
};
class SequentialFileImpl : public SequentialFile {
 public:
  explicit SequentialFileImpl(FileState* file) : file_(file), pos_(0) {
    file_->Ref();
  }
  ~SequentialFileImpl() override { file_->Unref(); };

  Status Read(size_t n, Slice* result, char* scratch) override {
    Status s = file_->Read(pos_, n, result, scratch);
    if (s.ok()) {
      pos_ += result->size();
    }
    return s;
  }
  Status Skip(uint64_t n) override {
    if (pos_ > file_->Size()) {
      return Status::IOError("pos_ > file_->Size()");
    }
    const uint64_t available = file_->Size() - pos_;
    if (n > available) {
      n = available;
    }
    pos_ += n;
    return Status::OK();
  }

 private:
  FileState* file_;
  uint64_t pos_;
};

class RandomAccessFileImpl : public RandomAccessFile {
 public:
  explicit RandomAccessFileImpl(FileState* file) : file_(file) { file->Ref(); }

  ~RandomAccessFileImpl() override { file_->Unref(); }

  Status Read(uint64_t offset, size_t n, Slice* result,
              char* scratch) const override {
    return file_->Read(offset, n, result, scratch);
  }

 private:
  FileState* file_;
};

class WritableFileImpl : public WritableFile {
 public:
  explicit WritableFileImpl(FileState* file) : file_(file) { file_->Ref(); }
  ~WritableFileImpl() override { file_->Unref(); }

  Status Append(const Slice& data) override { return file_->Append(data); }

  Status Close() override { return Status::OK(); }
  Status Flush() override { return Status::OK(); }
  Status Sync() override { return Status::OK(); }

 private:
  FileState* file_;
};

class NoOpLogger : public Logger {
 public:
  void Logv(const char* format, std::va_list ap) override {}
};
class InMemoryEnv : public EnvWrapper {
 public:
  explicit InMemoryEnv(Env* base_env) : EnvWrapper(base_env) {}
  ~InMemoryEnv() override {
    for (const auto& kvp : file_map_) {
      kvp.second->Unref();
    }
  }

  Status NewSequentialFile(const std::string& fname,
                           SequentialFile** result) override {
    MutexLock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      *result = nullptr;
      return Status::IOError(fname, "File not found");
    }
    *result = new SequentialFileImpl(file_map_[fname]);
    return Status::OK();
  }
  Status NewRandomAccessFile(const std::string& fname,
                             RandomAccessFile** result) override {
    MutexLock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      *result = nullptr;
      return Status::IOError(fname, "File not found");
    }
    *result = new RandomAccessFileImpl(file_map_[fname]);
    return Status::OK();
  }

  Status NewWritableFile(const std::string& fname,
                         WritableFile** result) override {
    MutexLock lock(&mutex_);
    FileSystem::iterator it = file_map_.find(fname);
    FileState* file;
    if (it == file_map_.end()) {
      file = new FileState();
      file->Ref();
      file_map_[fname] = file;  // add new file
    } else {
      file = it->second;
      file->Truncate();  // clear
    }
    *result = new WritableFileImpl(file);
    return Status::OK();
  }

  Status NewAppendableFile(const std::string& fname,
                           WritableFile** result) override {
    // leveldb 用的不同的写法
    MutexLock lock(&mutex_);
    FileSystem::iterator it = file_map_.find(fname);
    FileState* file;
    if (it == file_map_.end()) {
      file = new FileState();
      file->Ref();
      file_map_[fname] = file;
    } else {
      file = it->second;
    }
    *result = new WritableFileImpl(file);
    return Status::OK();
  }

  bool FileExists(const std::string& fname) override {
    MutexLock lock(&mutex_);
    return file_map_.find(fname) != file_map_.end();
  }

  Status GetChildren(const std::string& dir,
                     std::vector<std::string>* result) override {
    MutexLock lock(&mutex_);
    result->clear();
    for (const auto& kvp : file_map_) {
      const std::string& filename = kvp.first;
      // absolute path to relative path
      if (filename.size() >= dir.size() + 1 && filename[dir.size()] == '/' &&
          Slice(filename).starts_with(Slice(dir))) {
        result->push_back(filename.substr(dir.size() + 1));
      }
    }
    return Status::OK();
  }
  Status RemoveFile(const std::string& fname) override {
    MutexLock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      return Status::IOError(fname, "File not found");
    }
    RemoveFileInternal(fname);
    return Status::OK();
  }
  Status CreateDir(const std::string& dirname) override { return Status::OK(); }

  Status RemoveDir(const std::string& dirname) override { return Status::OK(); }

  Status GetFileSize(const std::string& fname, uint64_t* file_size) override {
    MutexLock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      return Status::IOError(fname, "File not found");
    }
    *file_size = file_map_[fname]->Size();
    return Status::OK();
  }
  Status RenameFile(const std::string& src,
                    const std::string& target) override {
    MutexLock lock(&mutex_);
    if (file_map_.find(src) == file_map_.end()) {
      return Status::IOError(src, "File not found");
    }
    RemoveFileInternal(target);
    file_map_[target] = file_map_[src];
    file_map_.erase(src);
    return Status::OK();
  }

  Status LockFile(const std::string& fname, FileLock** lock) override {
    *lock = new FileLock;  // seem do nothing
    return Status::OK();
  }

  Status UnlockFile(FileLock* lock) override {
    delete lock;
    return Status::OK();
  }

  Status GetTestDirectory(std::string* path) override {
    *path = "/test";
    return Status::OK();
  }

  Status NewLogger(const std::string& fname, Logger** result) override {
    *result = new NoOpLogger;  // do nothing
    return Status::OK();
  }

 private:
  void RemoveFileInternal(const std::string& fname)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    if (file_map_.find(fname) == file_map_.end()) {
      return;
    }
    file_map_[fname]->Unref();
    file_map_.erase(fname);
  }

  // filename to FileState
  typedef std::map<std::string, FileState*> FileSystem;
  port::Mutex mutex_;
  FileSystem file_map_ GUARDED_BY(mutex_);
};

}  // namespace
Env* NewMemEnv(Env* base_env) { return new InMemoryEnv(base_env); }

}  // namespace leveldb
