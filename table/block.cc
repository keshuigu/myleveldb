
#include "table/block.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "leveldb/comparator.h"
#include "table/format.h"
#include "util/coding.h"
#include "util/logging.h"

namespace leveldb {
// NumRestarts 存放于块的末尾4字节中
inline uint32_t Block::NumRestarts() const {
  assert(size_ > sizeof(uint32_t));
  return DecodeFixed32(data_ + size_ - sizeof(uint32_t));
}

Block::Block(const BlockContents& contents)
    : data_(contents.data.data()),
      size_(contents.data.size()),
      owned_(contents.heap_allocated) {
  if (size_ < sizeof(uint32_t)) {
    size_ = 0;
  } else {
    // 块的尾部形式如下：
    //     restarts: uint32[num_restarts]
    //     num_restarts: uint32
    // restarts[i] 包含块中第 i 个重启点的偏移量。
    restart_offset_ = size_ - (1 + NumRestarts()) * sizeof(uint32_t);
  }
}

Block::~Block() {
  if (owned_) {
    delete[] data_;
  }
}

// 辅助例程：从 "p" 开始解码下一个块条目，
// 分别将共享键字节数、非共享键字节数和值的长度存储在
// "*shared"、"*non_shared" 和 "*value_length" 中。不会解引用超过 "limit"
// 的内容。
//
// 如果检测到任何错误，则返回
// nullptr。否则，返回指向key_delta的指针（刚好在三个解码值之后）。
static inline const char* DecodeEntry(const char* p, const char* limit,
                                      uint32_t* shared, uint32_t* non_shared,
                                      uint32_t* value_length) {
  // "*shared"、"*non_shared" 和 "*value_length" 为varint32 最短1字节
  if (limit - p < 3) {
    return nullptr;
  }
  *shared = reinterpret_cast<const uint8_t*>(p)[0];
  *non_shared = reinterpret_cast<const uint8_t*>(p)[1];
  *value_length = reinterpret_cast<const uint8_t*>(p)[2];
  // 如果都是1字节varint32，快速处理
  // 判断方式是最高位是否为1
  if ((*shared | *non_shared | *value_length) < 128) {
    p += 3;
  } else {
    if ((p = GetVarint32Ptr(p, limit, shared)) == nullptr) {
      return nullptr;
    }
    if ((p = GetVarint32Ptr(p, limit, non_shared)) == nullptr) {
      return nullptr;
    }
    if ((p = GetVarint32Ptr(p, limit, value_length)) == nullptr) {
      return nullptr;
    }
  }
  // 不够读取存放的key_delta的长度了
  if (static_cast<uint32_t>(limit - p) < (*non_shared + *value_length)) {
    return nullptr;
  }
  return p;
}

class Block::Iter : public Iterator {
 public:
  Iter(const Comparator* comparator, const char* data, uint32_t restarts,
       uint32_t num_restarts)
      : comparator_(comparator),
        data_(data),
        restarts_(restarts),
        num_restarts_(num_restarts) {
    assert(num_restarts_ > 0);
  }

  bool Valid() const override { return current_ < restarts_; }
  Status status() const override { return status_; }
  Slice key() const override {
    assert(Valid());
    return key_;
  }
  Slice value() const override {
    assert(Valid());
    return value_;
  }

  void Next() override {
    assert(Valid());
    ParseNextKey();
  }

  void Prev() override {
    assert(Valid());
    const uint32_t original = current_;
    while (GetRestartPoint(restart_index_) >= original) {
      // 在第一个词条进行 prev 导致无效
      if (restart_index_ == 0) {
        current_ = restarts_;
        restart_index_ = num_restarts_;
        return;
      }
      restart_index_--;
    }
    SeekToRestartPoint(restart_index_);
    do {
      // 找到original的前一个词条
    } while (ParseNextKey() && NextEntryOffset() < original);
  }

  void Seek(const Slice& target) override {
    // 二分查找最后一个小于target的重启点
    uint32_t left = 0;
    uint32_t right = num_restarts_ - 1;
    int current_key_compare = 0;

    // 利用key_作为二分起点
    if (Valid()) {
      current_key_compare = Compare(key_, target);
      if (current_key_compare < 0) {
        // key_ 小
        left = restart_index_;
      } else if (current_key_compare > 0) {
        right = restart_index_;
      } else {
        // equal
        return;
      }
    }

    while (left < right) {
      uint32_t mid = (left + right + 1) / 2;
      uint32_t region_offset = GetRestartPoint(mid);
      uint32_t shared, non_shared, value_length;
      const char* key_ptr =
          DecodeEntry(data_ + region_offset, data_ + restarts_, &shared,
                      &non_shared, &value_length);
      if (key_ptr == nullptr || (shared != 0)) {
        CorruptionError();
        return;
      }
      Slice mid_key(key_ptr, non_shared);
      if (Compare(mid_key, target) < 0) {
        left = mid;  // 这是有效答案
      } else {
        right = mid - 1;
      }
    }
    // 感觉 current_key_compare == 0 没用
    // Valid()表明当前key有后继
    assert(current_key_compare == 0 || Valid());
    // 如果是当前位置且满足key_ < target，则不需要重新找重启点
    bool skip_seek = left == restart_index_ && current_key_compare < 0;
    if (!skip_seek) {
      SeekToRestartPoint(left);
    }
    while (true) {
      if (!ParseNextKey()) {
        return;
      }
      // 目标
      if (Compare(key_, target) >= 0) {
        return;
      }
    }
  }

  void SeekToFirst() override {
    SeekToRestartPoint(0);
    ParseNextKey();
  }

  void SeekToLast() override {
    SeekToRestartPoint(num_restarts_ - 1);
    while (ParseNextKey() && NextEntryOffset() < restarts_) {
      // Keep skipping
    }
  }

 private:
  const Comparator* const comparator_;
  const char* const data_;   // underlying block contents
  uint32_t const restarts_;  // restart array 的起始位置
  uint32_t const num_restarts_;

  // data_ 中当前词条的offset >= restarts if !Valid
  uint32_t current_;
  uint32_t restart_index_;
  std::string key_;  // 当前key
  Slice value_;      // ?
  Status status_;

  inline int Compare(const Slice& a, const Slice& b) const {
    return comparator_->Compare(a, b);
  }

  // Return the offset in data_ just past the end of the current entry.
  inline uint32_t NextEntryOffset() const {
    return (value_.data() + value_.size()) - data_;
  }

  uint32_t GetRestartPoint(uint32_t index) {
    assert(index < num_restarts_);
    return DecodeFixed32(data_ + restarts_ + index * sizeof(uint32_t));
  }

  void SeekToRestartPoint(uint32_t index) {
    key_.clear();
    restart_index_ = index;
    // current_ will be fixed by ParseNextKey();
    // ParseNextKey() starts at the end of value_, so set value_ accordingly
    uint32_t offset = GetRestartPoint(index);
    value_ = Slice(data_ + offset,
                   0);  // 当前key_不对应数据，执行ParseNextKey()读取第一个词条
  }

  void CorruptionError() {
    current_ = restarts_;
    restart_index_ = num_restarts_;
    status_ = Status::Corruption("bad entry in block");
    key_.clear();
    value_.clear();
  }

  bool ParseNextKey() {
    current_ = NextEntryOffset();  // 在此之前已经修改restart_index_ 和 value_
    const char* p = data_ + current_;
    const char* limit = data_ + restarts_;  // 数据区域的指针上限位置
    // 无next
    if (p >= limit) {
      current_ = restarts_;
      restart_index_ = num_restarts_;
      return false;
    }

    uint32_t shared, non_shared, value_length;
    p = DecodeEntry(p, limit, &shared, &non_shared, &value_length);
    if (p == nullptr || key_.size() < shared) {
      CorruptionError();
      return false;
    } else {
      // 保留共同前缀
      key_.resize(shared);
      // 添加后缀
      key_.append(p, non_shared);
      // p已经跳过三个数值，位于key_delta
      value_ = Slice(p + non_shared, value_length);
      // ?找下一个重启点
      // 当位于restart_index_ + 1重启点时
      // GetRestartPoint(restart_index_ + 1) == current_
      // 循环执行++restart_index_
      while (restart_index_ + 1 < num_restarts_ &&
             GetRestartPoint(restart_index_ + 1) < current_) {
        ++restart_index_;
      }
      return true;
    }
  }
};

Iterator* Block::NewIterator(const Comparator* comparator) {
  if (size_ < sizeof(uint32_t)) {
    return NewErrorIterator(Status::Corruption("bad block contents"));
  }
  const uint32_t num_restarts = NumRestarts();
  if (num_restarts == 0) {
    return NewEmptyIterator();
  } else {
    return new Iter(comparator, data_, restart_offset_, num_restarts);
  }
}

}  // namespace leveldb
