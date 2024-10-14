#include "util/logging.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include "leveldb/env.h"
#include "leveldb/slice.h"

namespace leveldb {

void AppendNumberTo(std::string* str, uint64_t num) {
  char buf[30];
  std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(num));
  str->append(buf);
}

void AppendEscapedStringTo(std::string* str, const Slice& value) {
  for (size_t i = 0; i < value.size(); i++) {
    char c = value[i];
    if (c >= ' ' && c <= '~') {  // from 0x20 to 0x7e
      str->push_back(c);
    } else {
      char buf[10];
      std::snprintf(buf, sizeof(buf), "\\x%02x",
                    static_cast<unsigned char>(c) & 0xff);
      str->append(buf);
    }
  }
}

std::string NumberToString(uint64_t num) {
  std::string r;
  AppendNumberTo(&r, num);
  return r;
}

std::string EscapeString(const Slice& value) {
  std::string r;
  AppendEscapedStringTo(&r, value);
  return r;
}

bool ConsumeDecimalNumber(Slice* in, uint64_t* val) {
  constexpr const uint64_t kMaxUint64 = std::numeric_limits<uint64_t>::max();
  constexpr const char kLastDigitOfMaxUint64 =
      '0' + static_cast<char>(kMaxUint64 % 10);  // 获取最低位数字

  uint64_t value = 0;
  const uint8_t* start = reinterpret_cast<const uint8_t*>(in->data());
  const uint8_t* end = start + in->size();
  const uint8_t* current = start;
  for (; current != end; current++) {
    const uint8_t ch = *current;
    if (ch < '0' || ch > '9') {
      break;  // illegal
    }
    // overflow check
    if (value > kMaxUint64 / 10 ||
        (value == kMaxUint64 / 10 && ch > kLastDigitOfMaxUint64)) {
      return false;  // 溢出了
    }
    value = (value * 10) + (ch - '0');
  }

  *val = value;
  size_t digits_consumed = current - start;
  in->remove_prefix(digits_consumed);
  return digits_consumed != 0;
}
}  // namespace leveldb
