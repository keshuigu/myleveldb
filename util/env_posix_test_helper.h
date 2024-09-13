#ifndef STORAGE_LEVELDB_UTIL_ENV_POSIX_TEST_HELPER_H_
#define STORAGE_LEVELDB_UTIL_ENV_POSIX_TEST_HELPER_H_

namespace leveldb {
class EnvPosixTest; // 在env_posix_test.cc 中定义
class EnvPosixTestHelper {
  // 实现在env_posix.cc中
 private:
  friend class EnvPosixTest;

  // 在创建Env前调用
  // 设置最大的只读文件数量
  static void SetReadOnlyFDLimit(int limit);

  // 在创建Env前调用
  // 设置最大的通过mmap映射的只读文件数量
  static void SetReadOnlyMMapLimit(int limit);
};
}  // namespace leveldb

#endif