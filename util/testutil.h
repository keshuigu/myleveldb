#ifndef STORAGE_LEVELDB_UTIL_TESTUTIL_H_
#define STORAGE_LEVELDB_UTIL_TESTUTIL_H_

#include <cstddef>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "helpers/memenv/memenv.h"
#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "util/random.h"

namespace leveldb {
namespace test {
MATCHER(IsOK, "") { return arg.ok(); }  // 自定义规则

#define EXPECT_LEVELDB_OK(expression) \
  EXPECT_THAT(expression, leveldb::test::IsOK())

#define ASSERT_LEVELDB_OK(expression) \
  ASSERT_THAT(expression, leveldb::test::IsOK())

inline int RandomSeed() {
  return testing::UnitTest::GetInstance()->random_seed();
}

// 生成长度为len的随机字符串存于dst，返回引用该dst的Slice
Slice RandomString(Random* rnd, int len, std::string* dst);

// 不同于RandomString，产生一些特定字符
std::string RandomKey(Random* rnd, int len);

// 将*dst填充为N个 (len * compressed_fraction)
// 长的小字符串，返回引用该dst的Slice
Slice CompressibleString(Random* rnd, double compressed_fraction, size_t len,
                         std::string* dst);

// 允许注入错误的环境

class ErrorEnv : public EnvWrapper {
 public:
  bool writable_file_error_;
  int num_writable_file_errors_;

  ErrorEnv()
      : EnvWrapper(NewMemEnv(Env::Default())),
        writable_file_error_(false),
        num_writable_file_errors_(0) {}
  ~ErrorEnv() override { delete target(); }

  Status NewWritableFile(const std::string& f, WritableFile** r) override {
    if (writable_file_error_) {
      ++num_writable_file_errors_;
      *r = nullptr;
      return Status::IOError(f, "fake error");
    }
    return target()->NewWritableFile(f, r);
  }

  Status NewAppendableFile(const std::string& f, WritableFile** r) override {
    if (writable_file_error_) {
      ++num_writable_file_errors_;
      *r = nullptr;
      return Status::IOError(f, "fake error");
    }
    return target()->NewAppendableFile(f, r);
  }
};
}  // namespace test

}  // namespace leveldb

#endif