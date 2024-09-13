// posix 标准下的logger实现
#ifndef STORAGE_LEVELDB_UTIL_POSIX_LOGGER_H_
#define STORAGE_LEVELDB_UTIL_POSIX_LOGGER_H_

#include <sys/time.h>

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <thread>

#include "leveldb/env.h"

namespace leveldb {
class PosixLogger final : public Logger {
 public:
  explicit PosixLogger(std::FILE* fp) : fp_(fp) {
    assert(fp != nullptr);
  }  // 先执行fp_(fp)
  ~PosixLogger() override { std::fclose(fp_); }

  void Logv(const char* format, std::va_list ap) override {
    // 记录时间
    struct ::timeval now_timeval;
    ::gettimeofday(&now_timeval, nullptr);
    const std::time_t now_seconds = now_timeval.tv_sec;
    struct std::tm now_components;
    ::localtime_r(&now_seconds, &now_components);
    // 记录线程ID
    constexpr int kMaxThreadIdSize = 32;
    std::ostringstream thread_stream;
    thread_stream << std::this_thread::get_id();
    std::string thread_id = thread_stream.str();
    if (thread_id.size() > kMaxThreadIdSize) {
      thread_id.resize(kMaxThreadIdSize);
    }
    // 首先存于栈空间
    // 存不下则分配堆空间
    constexpr int kStackBufferSize = 512;
    char stack_buffer[kStackBufferSize];
    static_assert(sizeof(stack_buffer) == static_cast<size_t>(kStackBufferSize),
                  "sizeof(char) is expected to be 1 in C++");
    int dynamic_buffer_size = 0;
    for (int iteration = 0; iteration < 2; iteration++) {
      // iteration == 0 -> stack
      // iteration == 1 -> alloc
      const int buffer_size =
          (iteration == 0) ? kStackBufferSize : dynamic_buffer_size;
      char* const buffer =
          (iteration == 0) ? stack_buffer : new char[dynamic_buffer_size];
      // time + id
      // 至多28字节+len(id)
      int buffer_offset = std::snprintf(
          buffer, buffer_size, "%04d/%02d/%02d-%02d:%02d:%02d.%06d %s ",
          now_components.tm_year + 1900, now_components.tm_mon + 1,
          now_components.tm_mday, now_components.tm_hour, now_components.tm_min,
          now_components.tm_sec, static_cast<int>(now_timeval.tv_usec),
          thread_id.c_str());
      assert(buffer_offset <= 28 + kMaxThreadIdSize);
      static_assert(28 + kMaxThreadIdSize < kStackBufferSize,
                    "stack-allocated buffer may not fit the message header");
      assert(buffer_offset < buffer_size);

      std::va_list ap_copy;
      va_copy(ap_copy, ap);
      // 如果不够长，会返回需要的长度
      buffer_offset += std::vsnprintf(
          buffer + buffer_offset, buffer_size - buffer_offset, format, ap_copy);
      va_end(ap_copy);

      if (buffer_offset >= buffer_size - 1) {
        if (iteration == 0) {
          dynamic_buffer_size = buffer_offset + 2;  // 2 是 新行符 + 终止空字符
          continue;
        }
        // 不应执行到此处
        assert(false);
        buffer_offset = buffer_size - 1;
      }

      // 增加可能需要的新行符
      if (buffer[buffer_offset - 1] != '\n') {
        buffer[buffer_offset] = '\n';
        ++buffer_offset;
      }

      // 不影响结果，但是似乎没有考虑结尾的空字符
      assert(buffer_offset <= buffer_size);
      std::fwrite(buffer, 1, buffer_offset, fp_);
      std::fflush(fp_);
      if (iteration != 0) {
        delete[] buffer;
      }
      break;  // 如果iteration -= 0 并且执行成功，不用执行第二次迭代
    }
  }

 private:
  std::FILE* const fp_;
};

}  // namespace leveldb

#endif