#include "leveldb/comparator.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>

#include "leveldb/slice.h"
#include "util/no_destructor.h"

namespace leveldb {
Comparator::~Comparator() = default;
namespace {
class BytewiseComparatorImpl : public Comparator {
 public:
  BytewiseComparatorImpl() = default;
  const char* Name() const override { return "leveldb.BytewiseComparator"; }
  int Compare(const Slice& a, const Slice& b) const override {
    return a.compare(b);
  }

  void FindShortestSeparator(std::string* start,
                             const Slice& limit) const override {
    // 找共同前缀
    size_t min_length = std::min(start->size(), limit.size());
    size_t diff_index = 0;
    while ((diff_index < min_length) &&
           ((*start)[diff_index] == limit[diff_index])) {
      diff_index++;  // 找不同的起始位置
    }
    // 其中一个是另一个的前缀时不做操作
    if (diff_index < min_length) {
      uint8_t diff_type = static_cast<uint8_t>((*start)[diff_index]);
      // 只有第一个不同的char存在且start此处char比limit至少小2, 才会进行操作
      if (diff_type < static_cast<uint8_t>(0xff) &&
          diff_type + 1 < static_cast<uint8_t>(limit[diff_index])) {
        (*start)[diff_index]++;
        start->resize(diff_index + 1);
        assert(Compare(*start, limit) < 0);
      }
    }
  }

  void FindShortSuccessor(std::string* key) const override {
    size_t n = key->size();
    for (size_t i = 0; i < n; i++) {
      const uint8_t byte = (*key)[i];
      if (byte != static_cast<uint8_t>(0xff)) {
        (*key)[i] = byte + 1;
        key->resize(i + 1);
        return;
      }
    }
  }
};
}  // namespace
const Comparator* BytewiseComparator() {
  static NoDestructor<BytewiseComparatorImpl> singleton;
  return singleton.get();
}
}  // namespace leveldb