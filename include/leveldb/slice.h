// 定义切片，也就是leveldb中的基本单位
// const方法保证多线程安全，非const方法需额外的外部同步

#ifndef STORAGE_LEVELDB_INCLUDE_SLICE_H_
#define STORAGE_LEVELDB_INCLUDE_SLICE_H_

#include <cassert>  // 断言
#include <cstddef>
#include <cstring>
#include <string>

#include "leveldb/export.h"

namespace leveldb {

class LEVELDB_EXPORT Slice {
 public:
  Slice() : data_(""), size_(0) {}
  Slice(const char* d, size_t n) : data_(d), size_(n) {}
  Slice(const std::string& s) : data_(s.data()), size_(s.size()) {}
  Slice(const char* s) : data_(s), size_(strlen(s)) {}
  Slice(const Slice&) = default;  // 默认拷贝构造为字节序拷贝
  Slice& operator=(const Slice&) = default;  // 默认赋值拷贝构造为字节序拷贝

  const char* data() const { return data_; }

  size_t size() const { return size_; }

  bool empty() const { return size_ == 0; }

  const char* begin() const { return data(); }
  const char* end() const { return data() + size(); }

  char operator[](size_t n) const {
    assert(n < size());
    return data_[n];
  }

  void clear() {
    data_ = "";
    size_ = 0;
  }

  void remove_prefix(size_t n) {
    assert(n <= size());
    data_ += n;
    size_ -= n;
  }

  std::string ToString() const { return std::string(data_, size_); }

  int compare(const Slice& b) const;

  bool starts_with(const Slice& x) const {
    return ((size_ > x.size_) && (memcmp(data_, x.data_, x.size_) == 0));
  }

 private:
  const char* data_;
  size_t size_;
};

inline bool operator==(const Slice& x, const Slice& y) {
  return ((x.size() == y.size()) && memcmp(x.data(), y.data(), x.size()) == 0);
}
inline bool operator!=(const Slice& x, const Slice& y) { return !(x == y); }
inline int Slice::compare(const Slice& b) const {
  const size_t min_len = (size_ < b.size_) ? size_ : b.size_;
  int r = memcmp(data_, b.data_, min_len);
  if (r == 0) {
    if (size_ < b.size_) {
      r = -1;
    } else if (size_ > b.size_) {
      r = +1;
    }
  }
  return r;
}
}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_SLICE_H_