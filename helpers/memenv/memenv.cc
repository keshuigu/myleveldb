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
  FileState(/* args */);
  void Truncate() {
    MutexLock lock(&blocks_mutex_);
    for (char*& block : blocks_) {
      delete[] block;
    }
    blocks_.clear();
    size_ = 0;
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

}  // namespace

}  // namespace leveldb
