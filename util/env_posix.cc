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
      return true;
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

}  // namespace
}  // namespace leveldb
