#include "util/coding.h"

#include <vector>
#include "leveldb/slice.h"
#include "gtest/gtest.h"

namespace leveldb {
TEST(Coding, Fixed32) {
  std::string s;
  for (uint32_t v = 0; v < 100000; v++) {
    PutFixed32(&s, v);
  }
  const char *p = s.data();
  for (uint32_t v = 0; v < 100000; v++) {
    uint32_t res = DecodeFixed32(p);
    ASSERT_EQ(res, v);
    p += sizeof(uint32_t);
  }
}

TEST(Coding, Fixed64) {
  std::string s;
  for (uint32_t pow = 0; pow <= 63; pow++) {
    uint64_t v = static_cast<uint64_t>(1) << pow;
    PutFixed64(&s, v - 1);
    PutFixed64(&s, v + 0);
    PutFixed64(&s, v + 1);
  }
  const char *p = s.data();
  for (uint32_t pow = 0; pow <= 63; pow++) {
    uint64_t v = static_cast<uint64_t>(1) << pow;
    uint64_t res = DecodeFixed64(p);
    ASSERT_EQ(res, v - 1);
    p += sizeof(uint64_t);

    res = DecodeFixed64(p);
    ASSERT_EQ(res, v + 0);
    p += sizeof(uint64_t);

    res = DecodeFixed64(p);
    ASSERT_EQ(res, v + 1);
    p += sizeof(uint64_t);
  }
}

TEST(Coding, EncodingOutput) {
  std::string dst;
  PutFixed32(&dst, 0x04030201);
  // 验证小端序
  ASSERT_EQ(4, dst.size());
  ASSERT_EQ(0x01, static_cast<int>(dst[0]));
  ASSERT_EQ(0x02, static_cast<int>(dst[1]));
  ASSERT_EQ(0x03, static_cast<int>(dst[2]));
  ASSERT_EQ(0x04, static_cast<int>(dst[3]));

  dst.clear();
  PutFixed64(&dst, 0x0807060504030201ull);
  ASSERT_EQ(8, dst.size());
  ASSERT_EQ(0x01, static_cast<int>(dst[0]));
  ASSERT_EQ(0x02, static_cast<int>(dst[1]));
  ASSERT_EQ(0x03, static_cast<int>(dst[2]));
  ASSERT_EQ(0x04, static_cast<int>(dst[3]));
  ASSERT_EQ(0x05, static_cast<int>(dst[4]));
  ASSERT_EQ(0x06, static_cast<int>(dst[5]));
  ASSERT_EQ(0x07, static_cast<int>(dst[6]));
  ASSERT_EQ(0x08, static_cast<int>(dst[7]));
}

TEST(Coding, Varint32) {
  std::string s;
  for (uint32_t i = 0; i < (32 * 32); i++) {
    uint32_t v = (i / 32) << (i % 32);
    PutVarint32(&s, v);
  }

  const char *p = s.data();
  const char *limit = p + s.size();
  for (uint32_t i = 0; i < (32 * 32); i++) {
    uint32_t v = (i / 32) << (i % 32);
    uint32_t res;
    const char *start = p;
    p = GetVarint32Ptr(p, limit, &res);
    ASSERT_TRUE(p != nullptr);
    ASSERT_EQ(v, res);
    ASSERT_EQ(VarintLength(res), p - start);
  }
  ASSERT_EQ(p, limit);
}

TEST(Coding, Varint64) {
  // Construct the list of values to check
  std::vector<uint64_t> values;
  // Some special values
  values.push_back(0);
  values.push_back(100);
  values.push_back(~static_cast<uint64_t>(0));
  values.push_back(~static_cast<uint64_t>(0) - 1);
  for (uint32_t k = 0; k < 64; k++) {
    // Test values near powers of two
    const uint64_t power = 1ull << k;
    values.push_back(power);
    values.push_back(power - 1);
    values.push_back(power + 1);
  }

  std::string s;
  for (size_t i = 0; i < values.size(); i++) {
    PutVarint64(&s, values[i]);
  }

  const char *p = s.data();
  const char *limit = p + s.size();
  for (size_t i = 0; i < values.size(); i++) {
    ASSERT_TRUE(p < limit);
    uint64_t actual;
    const char *start = p;
    p = GetVarint64Ptr(p, limit, &actual);
    ASSERT_TRUE(p != nullptr);
    ASSERT_EQ(values[i], actual);
    ASSERT_EQ(VarintLength(actual), p - start);
  }
  ASSERT_EQ(p, limit);
}

TEST(Coding, Varint32Overflow) {
  uint32_t result;
  std::string input("\x81\x82\x83\x84\x85\x11");  // 6bytes
  ASSERT_TRUE(GetVarint32Ptr(input.data(), input.data() + input.size(),
                             &result) == nullptr);
}

TEST(Coding, Varint32Truncation) {
  uint32_t large_value = (1u << 31) + 100;
  std::string s;
  PutVarint32(&s, large_value);
  uint32_t result;
  // 长度不足 被截断
  for (size_t len = 0; len < s.size() - 1; len++) {
    ASSERT_TRUE(GetVarint32Ptr(s.data(), s.data() + len, &result) == nullptr);
  }
  ASSERT_TRUE(GetVarint32Ptr(s.data(), s.data() + s.size(), &result) !=
              nullptr);
  ASSERT_EQ(large_value, result);
}

TEST(Coding, Varint64Overflow) {
  uint64_t result;
  std::string input(
      "\x81\x82\x83\x84\x85\x81\x82\x83\x84\x85\x11");  // 11 bytes
  ASSERT_TRUE(GetVarint64Ptr(input.data(), input.data() + input.size(),
                             &result) == nullptr);
}

TEST(Coding, Varint64Truncation) {
  uint64_t large_value = (1ull << 63) + 100ull;
  std::string s;
  PutVarint64(&s, large_value);
  uint64_t result;
  for (size_t len = 0; len < s.size() - 1; len++) {
    ASSERT_TRUE(GetVarint64Ptr(s.data(), s.data() + len, &result) == nullptr);
  }
  ASSERT_TRUE(GetVarint64Ptr(s.data(), s.data() + s.size(), &result) !=
              nullptr);
  ASSERT_EQ(large_value, result);
}

TEST(Coding, Strings) {
  std::string s;
  PutLengthPrefixedSlice(&s, Slice(""));
  PutLengthPrefixedSlice(&s, Slice("foo"));
  PutLengthPrefixedSlice(&s, Slice("bar"));
  PutLengthPrefixedSlice(&s, Slice(std::string(200, 'x')));

  Slice input(s);
  Slice v;
  ASSERT_TRUE(GetLengthPrefixedSlice(&input, &v));
  ASSERT_EQ("", v.ToString());
  ASSERT_TRUE(GetLengthPrefixedSlice(&input, &v));
  ASSERT_EQ("foo", v.ToString());
  ASSERT_TRUE(GetLengthPrefixedSlice(&input, &v));
  ASSERT_EQ("bar", v.ToString());
  ASSERT_TRUE(GetLengthPrefixedSlice(&input, &v));
  ASSERT_EQ(std::string(200, 'x'), v.ToString());
  ASSERT_EQ("", input.ToString());
}

}  // namespace leveldb
