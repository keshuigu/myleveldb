#ifndef STORAGE_LEVELDB_UTIL_RANDOM_H_
#define STORAGE_LEVELDB_UTIL_RANDOM_H_

#include <cstdint>

namespace leveldb {
class Random {
 public:
  explicit Random(uint32_t s) : seed_(s & 0x7fffffffu) {
    if (seed_ == 0 || seed_ == 2147483647L) {
      seed_ = 1;
    }
  }

  uint32_t Next() {
    // 线性同余法
    static const uint32_t M = 2147483647L;  // 2^31-1
    static const uint64_t A = 16807;        // bits 14, 8, 7, 5, 2, 1, 0
    // seed_ = (seed_ * A) % M,    其中 M = 2^31-1
    //
    // seed_ 不能为零或 M，否则所有后续计算的值将分别为零或 M。
    // 对于所有其他值，seed_ 将循环遍历 [1,M-1] 中的每个数字。
    uint64_t product = seed_ * A;
    // 使用 ((x << 31) % M) == x 的事实计算 (product % M)
    // (2 ^ 31) % M = 1
    // (x << 31) % M = (x * (2^31)) % M = (x * 1) % M
    seed_ = static_cast<uint32_t>((product >> 31) + (product & M));

    // 第一次减法可能会溢出 1 位，因此我们可能需要重复。
    // mod == M 是不可能的；使用 > 允许更快的基于符号位的测试。
    if (seed_ > M) {
      seed_ -= M;
    }
    return seed_;
  }
  // [0..n-1] 随机采样
  // REQUIRES: n > 0
  uint32_t Uniform(int n) { return Next() % n; }

  // 以约为 1/n 的概率返回True
  // REQUIRES: n > 0
  bool OneIn(int n) { return (Next() % n) == 0; }

  // 首先从[0,max_log]均匀选取base
  // 再从[0,2^max_log-1]均匀选取随机数
  // 因此小的数值更有可能选中
  uint32_t Skewed(int max_log) { return Uniform(1 << Uniform(max_log + 1)); }

 private:
  uint32_t seed_;
};

}  // namespace leveldb

#endif