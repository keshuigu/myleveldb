#ifndef STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_
#define STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_

#include <cstdint>
#include <vector>

#include "leveldb/slice.h"

namespace leveldb {
struct Options;

class BlockBuilder {
 public:
  explicit BlockBuilder(const Options* options);

  BlockBuilder(const BlockBuilder&) = delete;
  BlockBuilder& operator=(const BlockBuilder&) = delete;

  // 重置内容，就像 BlockBuilder 刚刚被构造一样。
  void Reset();

  // REQUIRES：自上次调用 Reset() 以来，未调用 Finish()。
  // REQUIRES：key 大于任何先前添加的 key
  void Add(const Slice& key, const Slice& value);

  // 完成块的构建并返回一个引用块内容的 Slice。
  // 返回的 Slice 在此构建器的生命周期内或直到调用 Reset() 之前保持有效。
  Slice Finish();

  // 返回我们正在构建的块的当前（未压缩）大小的估计值。
  size_t CurrentSizeEstimate() const;

  // 如果自上次调用 Reset() 以来未添加任何条目，则返回 true
  bool empty() const { return buffer_.empty(); }

 private:
  const Options* options_;
  std::string buffer_;              // dst buffer
  std::vector<uint32_t> restarts_;  // 重启点
  int counter_;    // 自上次重启以来发出的条目数量
  bool finished_;  // finish是否被调用
  std::string last_key_;
};

}  // namespace leveldb

#endif