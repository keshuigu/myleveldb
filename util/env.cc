#include "leveldb/env.h"

#include <cstdarg>

namespace leveldb {
Env::Env() = default;
Env::~Env() = default;
Status Env::NewAppendableFile(const std::string& fname, WritableFile** result) {
  return Status::NotSupported("NewAppendableFile", fname);
}
SequentialFile::~SequentialFile() = default;
RandomAccessFile::~RandomAccessFile() = default;
WritableFile::~WritableFile() = default;
Logger::~Logger() = default;
FileLock::~FileLock() = default;

void Log(Logger* info_log, const char* format, ...) {
  if (info_log != nullptr) {
    std::va_list ap;
    va_start(ap, format);  // format为可变参数前最后一个固定参数
    info_log->Logv(format, ap);
    va_end(ap);  // 结束可变参数处理
  }
}

static Status DoWriteStringToFile(Env* env, const Slice& data,
                                  const std::string& fname, bool should_sync) {
  WritableFile* file;
  Status s = env->NewWritableFile(fname, &file);
  if (!s.ok()) {
    return s;
  }
  s = file->Append(data);
  if (s.ok() && should_sync) {
    s = file->Sync();
  }
  if (s.ok()) {
    s = file->Close();
  }
  delete file;
  if (!s.ok()) {
    // line 38 or line 42 ?
    env->RemoveFile(
        fname);  // 也许不怎么会出错，那么不要接受返回值，否则ok覆盖s
  }
  return s;
}

Status WriteStringToFile(Env* env, const Slice& data,
                         const std::string& fname) {
  return DoWriteStringToFile(env, data, fname, false);
}

Status WriteStringToFileSync(Env* env, const Slice& data,
                             const std::string& fname) {
  return DoWriteStringToFile(env, data, fname, true);
}

Status ReadFileToString(Env* env, const std::string& fname, std::string* data) {
  data->clear();
  SequentialFile* file;
  Status s = env->NewSequentialFile(fname, &file);
  if (!s.ok()) {
    return s;
  }
  static const int kBufferSize = 8192;
  char* space = new char[kBufferSize];
  while (true) {
    Slice fragment;
    s = file->Read(kBufferSize, &fragment, space);
    if (!s.ok()) {
      break;
    }
    data->append(fragment.data(), fragment.size());
    if (fragment.empty()) {
      break;
    }
  }
  delete[] space;
  delete file;
  return s;
}
EnvWrapper::~EnvWrapper() {}

}  // namespace leveldb
