#include "leveldb/filter_policy.h"
#include "leveldb/slice.h"
#include "util/hash.h"

namespace leveldb {
namespace {
static uint32_t BloomHash(const Slice& key) {
  return Hash(key.data(), key.size(), 0xbc9f1d34);
}
}  // namespace
class BloomFilterPolicy : public FilterPolicy {
 public:
  explicit BloomFilterPolicy(int bits_per_key) : bits_per_key_(bits_per_key) {
    // 我们有意向下取整，以稍微减少探测成本
    k_ = static_cast<size_t>(bits_per_key * 0.69);  // 0.69 =~ ln(2)
    if (k_ < 1) {
      k_ = 1;
    } else if (k_ > 30) {
      k_ = 30;
    }
  }
  const char* Name() const override { return "leveldb.BuiltinBloomFilter2"; }

  void CreateFilter(const Slice* keys, int n, std::string* dst) const override {
    size_t bits = n * bits_per_key_;
    if (bits < 64) {
      bits = 64;
    }
    size_t bytes = (bits + 7) / 8;  // 上取整
    bits = bytes * 8;

    const size_t init_size = dst->size();
    dst->resize(init_size + bytes, 0);
    dst->push_back(static_cast<char>(k_));  // Remember # of probes in filter
    char* array = &(*dst)[init_size];
    for (int i = 0; i < n; i++) {
      uint32_t h = BloomHash(keys[i]);
      const uint32_t delta = (h >> 17) | (h << 15);  // 后17位换至最前
      for (size_t j = 0; j < k_; j++) {
        // 可操作的位一共bits个，h先对bits取模
        const uint32_t bitops = h % bits;
        // 取模结果bitops代表第bitops个位应该置1
        // 换算到char数组中就是下面的表达式
        array[bitops / 8] |= (1 << (bitops % 8));
        // 轮换h
        h += delta;
      }
    }
  }
  bool KeyMayMatch(const Slice& key, const Slice& filter) const override {
    const size_t len = filter.size();
    if (len < 2) {
      return false;  // filter 长度至少为2
    }
    const char* array = filter.data();
    const size_t bits = (len - 1) * 8;
    // 读取filter中的k_
    const size_t k = array[len - 1];
    if (k > 30) {
      // Reserved for potentially new encodings for short bloom filters.
      // Consider it a match.
      return true;
    }
    uint32_t h = BloomHash(key);
    const uint32_t delta = (h >> 17) | (h << 15);  // 后17位换至最前
    for (size_t j = 0; j < k; j++) {
      // 可操作的位一共bits个，h先对bits取模
      const uint32_t bitops = h % bits;
      // 取模结果bitops代表第bitops个位应该置1
      // 换算到char数组中就是下面的表达式
      if ((array[bitops / 8] & (1 << (bitops % 8))) == 0) {
        return false;
      }
      // 轮换h
      h += delta;
    }
    return true;
  }

 private:
  size_t bits_per_key_;
  size_t k_;  // 哈希函数的数量
};

const FilterPolicy* NewBloomFilterPolicy(int bits_per_key) {
  return new BloomFilterPolicy(bits_per_key);
}
}  // namespace leveldb
