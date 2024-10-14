#ifndef STORAGE_LEVELDB_INCLUDE_ENV_H_
#define STORAGE_LEVELDB_INCLUDE_ENV_H_

#include <cstdarg>
#include <cstdint>
#include <string>
#include <vector>

#include "leveldb/export.h"
#include "leveldb/status.h"
#include "leveldb/slice.h"


// 不考虑win32
// 弃用DeleteFile 和 DeleteDir

namespace leveldb {
class FileLock;
class Logger;
class RandomAccessFile;
class SequentialFile;
class WritableFile;
class LEVELDB_EXPORT Env {
 public:
  Env();

  Env(const Env&) = delete;
  Env& operator=(const Env&) = delete;

  virtual ~Env();

  // 根据当前OS生成默认Env
  // leveldb生命周期内Env不应该被删除
  static Env* Default();

  // 创建一个指向fname的文件对象，按顺序读取。
  // 成功后，将指向新文件的指针存储在*result中，并返回OK。
  // 失败时，将nullptr存储在*result中并返回非OK，例如：如果文件不存在，实现应该返回NotFound状态。
  //
  // 返回的文件一次只能由一个线程访问。
  virtual Status NewSequentialFile(const std::string& fname,
                                   SequentialFile** result) = 0;

  // 创建一个指向fname的文件对象，支持随机读取。
  // 成功后，将指向新文件的指针存储在*result中，并返回OK。
  // 失败时，将nullptr存储在*result中并返回非OK，例如：如果文件不存在，实现应该返回NotFound状态。
  //
  // 返回的文件一次只能由一个线程访问。
  virtual Status NewRandomAccessFile(const std::string& fname,
                                     RandomAccessFile** result) = 0;

  // 创建一个新文件fname及其文件对象，可写入。会删除任何同名的现有文件并创建新文件。
  // 成功时，将指向新文件的指针存储在*result中并返回OK。失败时，将nullptr存储在*results中并返回非OK。
  //
  // 返回的文件一次只能由一个线程访问。
  virtual Status NewWritableFile(const std::string& fname,
                                 WritableFile** result) = 0;

  // 创建一个新文件fname或者使用已有文件fname，可追加写入。
  // 成功时，将指向新文件的指针存储在*result中并返回OK。失败时，将nullptr存储在*results中并返回非OK。
  //
  // 返回的文件一次只能由一个线程访问。
  //
  // 环境可能不支持追加写入。
  virtual Status NewAppendableFile(const std::string& fname,
                                   WritableFile** result);

  virtual bool FileExists(const std::string& fname) = 0;

  // 以相对路径形式返回目录下的文件
  virtual Status GetChildren(const std::string& dir,
                             std::vector<std::string>* result) = 0;

  virtual Status RemoveFile(const std::string& fname) = 0;

  virtual Status CreateDir(const std::string& dirname) = 0;

  virtual Status RemoveDir(const std::string& dirname) = 0;

  virtual Status GetFileSize(const std::string& fname, uint64_t* file_size) = 0;

  virtual Status RenameFile(const std::string& src,
                            const std::string& target) = 0;

  // 锁定指定的文件。用于防止并发访问。失败时，将nullptr存储在*lock并返回非OK。
  //
  // 成功后，指向获取的锁的指针存储在*lock中，返回OK。调用者应调用UnlockFile(*lock)以解锁。如果该过程退出，锁将自动解锁。
  //
  // 如果其他人已经持有锁，该方法会直接失败，不会产生阻塞
  //
  // 如果命名文件不存在，会创建该文件。
  virtual Status LockFile(const std::string& fname, FileLock** lock) = 0;

  // 释放锁
  // REQUIERS: 锁由成功的LockFile获得
  // REQUIERS: 锁未被释放
  virtual Status UnlockFile(FileLock* lock) = 0;

  // 安排后台线程执行一次 (*function)(arg)
  // 不保证执行的先后顺序
  virtual void Schedule(void (*function)(void* arg), void* arg) = 0;

  // 开启新线程，执行完毕后线程销毁
  virtual void StartThread(void (*function)(void* arg), void* arg) = 0;

  virtual Status GetTestDirectory(std::string* path) = 0;

  // 日志文件
  virtual Status NewLogger(const std::string& fname, Logger** result) = 0;

  // 获取时间
  virtual uint64_t NowMicros() = 0;

  virtual void SleepForMicroseconds(int micros) = 0;
};

class LEVELDB_EXPORT SequentialFile {
 public:
  SequentialFile() = default;
  SequentialFile(const SequentialFile&) = delete;
  SequentialFile& operator=(const SequentialFile&) = delete;
  virtual ~SequentialFile();

  // 读取至多n个字节
  // result中的数据实际指向scratch，因此在result生命周期内，scratch需保持存活
  //
  // REQUIRES: 外部同步
  virtual Status Read(size_t n, Slice* result, char* scratch) = 0;

  // 跳过n个字节，保证不慢于Read
  // 如果不足n个字节，会停止于末端，返回ok
  //
  // REQUIRES: 外部同步
  virtual Status Skip(uint64_t n) = 0;
};

class LEVELDB_EXPORT RandomAccessFile {
 public:
  RandomAccessFile() = default;
  RandomAccessFile(const RandomAccessFile&) = delete;
  RandomAccessFile& operator=(const RandomAccessFile&) = delete;

  virtual ~RandomAccessFile();

  // 从offset起读取至多n个字节
  // result中的数据实际指向scratch，因此在result生命周期内，scratch需保持存活
  //
  // 并发安全
  virtual Status Read(uint64_t offset, size_t n, Slice* result,
                      char* scratch) const = 0;
};

class LEVELDB_EXPORT WritableFile {
 public:
  WritableFile() = default;
  WritableFile(const WritableFile&) = delete;
  WritableFile& operator=(const WritableFile&) = delete;

  virtual ~WritableFile();

  virtual Status Append(const Slice& data) = 0;
  virtual Status Close() = 0;
  virtual Status Flush() = 0;
  virtual Status Sync() = 0;
};

class LEVELDB_EXPORT Logger {
 public:
  Logger() = default;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  virtual ~Logger();
  virtual void Logv(const char* format,std::va_list ap) = 0;
};

class LEVELDB_EXPORT FileLock {
 public:
  FileLock() = default;
  FileLock(const FileLock&) = delete;
  FileLock& operator=(const FileLock&) = delete;

  virtual ~FileLock();
};

// __format__用于提示编译器在编译时检查函数调用的参数是否符合指定的格式

void Log(Logger* info_log, const char* format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((__format__(__printf__, 2, 3)))
#endif
    ;

LEVELDB_EXPORT Status WriteStringToFile(Env* env, const Slice& data,
                                        const std::string& fname);
// leveldb中声明在filename.cc中
// TODO 导出？移动声明位置？
Status WriteStringToFileSync(Env* env, const Slice& data,
                                        const std::string& fname);
LEVELDB_EXPORT Status ReadFileToString(Env* env, const std::string& fname,
                                       std::string* data);

// 封装Env，便于只修改部分函数来实现新的Env
class EnvWrapper : public Env {
 public:
  explicit EnvWrapper(Env* t) : target_(t) {}

  virtual ~EnvWrapper();

  Env* target() const { return target_; }

  Status NewSequentialFile(const std::string& f, SequentialFile** r) override {
    return target_->NewSequentialFile(f, r);
  }

  Status NewRandomAccessFile(const std::string& f,
                             RandomAccessFile** r) override {
    return target_->NewRandomAccessFile(f, r);
  }
  Status NewWritableFile(const std::string& f, WritableFile** r) override {
    return target_->NewWritableFile(f, r);
  }
  Status NewAppendableFile(const std::string& f, WritableFile** r) override {
    return target_->NewAppendableFile(f, r);
  }
  bool FileExists(const std::string& f) override {
    return target_->FileExists(f);
  }
  Status GetChildren(const std::string& dir,
                     std::vector<std::string>* r) override {
    return target_->GetChildren(dir, r);
  }
  Status RemoveFile(const std::string& f) override {
    return target_->RemoveFile(f);
  }
  Status CreateDir(const std::string& d) override {
    return target_->CreateDir(d);
  }
  Status RemoveDir(const std::string& d) override {
    return target_->RemoveDir(d);
  }
  Status GetFileSize(const std::string& f, uint64_t* s) override {
    return target_->GetFileSize(f, s);
  }
  Status RenameFile(const std::string& s, const std::string& t) override {
    return target_->RenameFile(s, t);
  }
  Status LockFile(const std::string& f, FileLock** l) override {
    return target_->LockFile(f, l);
  }
  Status UnlockFile(FileLock* l) override { return target_->UnlockFile(l); }
  void Schedule(void (*f)(void*), void* a) override {
    return target_->Schedule(f, a);
  }
  void StartThread(void (*f)(void*), void* a) override {
    return target_->StartThread(f, a);
  }
  Status GetTestDirectory(std::string* path) override {
    return target_->GetTestDirectory(path);
  }
  Status NewLogger(const std::string& fname, Logger** result) override {
    return target_->NewLogger(fname, result);
  }
  uint64_t NowMicros() override { return target_->NowMicros(); }
  void SleepForMicroseconds(int micros) override {
    target_->SleepForMicroseconds(micros);
  }

 private:
  Env* target_;
};

}  // namespace leveldb

#endif