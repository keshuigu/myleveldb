// BlockBuilder 生成的块中，键是前缀压缩的：
//
// 当我们存储一个键时，我们会去掉与前一个字符串共享的前缀。
// 这有助于显著减少空间需求。此外，每隔 K 个键，我们不进行前缀压缩，
// 而是存储整个键。我们称之为“重启点”。块的尾部存储所有重启点的偏移量，
// 并且在查找特定键时可以用于二分查找。值紧跟在对应的键之后按原样存储（不压缩）。
//
// 特定键值对的条目形式如下：
//     shared_bytes: varint32
//     unshared_bytes: varint32
//     value_length: varint32
//     key_delta: char[unshared_bytes]
//     value: char[value_length]
// 对于重启点，shared_bytes == 0。
//
// 块的尾部形式如下：
//     restarts: uint32[num_restarts]
//     num_restarts: uint32
// restarts[i] 包含块中第 i 个重启点的偏移量。
#include "table/block_builder.h"

#include <algorithm>
#include <cassert>

#include "leveldb/comparator.h"
#include "leveldb/options.h"
#include "util/coding.h"

namespace leveldb {

BlockBuilder::BlockBuilder(const Options* options)
    : options_(options), restarts_(), counter_(0), finished_(false) {
  assert(options_->block_restart_interval >= 1);
  restarts_.push_back(0);  // 第一个重启点偏移量为0
}
void BlockBuilder::Reset() {
  buffer_.clear();
  restarts_.clear();
  restarts_.push_back(0);
  counter_ = 0;
  finished_ = false;
  last_key_.clear();
}

size_t BlockBuilder::CurrentSizeEstimate() const {
  // 参照本文件头部注释
  return (buffer_.size() +                       // Raw data buffer
          restarts_.size() * sizeof(uint32_t) +  // Restart array
          sizeof(uint32_t));                     // Restart array length
}
Slice BlockBuilder::Finish() {
  // Append restart array
  for (size_t i = 0; i < restarts_.size(); i++) {
    PutFixed32(&buffer_, restarts_[i]);
  }
  PutFixed32(&buffer_, restarts_.size());
  finished_ = true;
  return Slice(buffer_);
}

void BlockBuilder::Add(const Slice& key, const Slice& value) {
  Slice last_key_piece(last_key_);
  assert(!finished_);
  assert(counter_ <= options_->block_restart_interval);
  assert(buffer_.empty() ||
         options_->comparator->Compare(key, last_key_piece) > 0);
  size_t shared = 0;
  if (counter_ < options_->block_restart_interval) {
    const size_t min_length = std::min(last_key_piece.size(), key.size());
    // 共同前缀
    while ((shared < min_length) && (last_key_piece[shared] == key[shared])) {
      shared++;
    }
  } else {
    restarts_.push_back(buffer_.size());
    counter_ = 0;
  }
  const size_t non_shared = key.size() - shared;
  PutVarint32(&buffer_, shared);
  PutVarint32(&buffer_, non_shared);
  PutVarint32(&buffer_, value.size());

  buffer_.append(key.data() + shared, non_shared);  // 只存入不同的字符
  buffer_.append(value.data(), value.size());

  last_key_.resize(shared);
  last_key_.append(key.data() + shared, non_shared);
  assert(Slice(last_key_) == key);
  counter_++;
}

}  // namespace leveldb
