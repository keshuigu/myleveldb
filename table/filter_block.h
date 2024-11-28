#ifndef STORAGE_LEVELDB_TABLE_FILTER_BLOCK_H_
#define STORAGE_LEVELDB_TABLE_FILTER_BLOCK_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "leveldb/slice.h"
#include "util/hash.h"

namespace leveldb {

class FilterPolicy;

// FilterBlockBuilder
// 用于构建特定表的所有过滤器。它生成一个字符串，该字符串作为一个特殊块存储在表中。
// 对 FilterBlockBuilder 的调用顺序必须符合正则表达式：
// (StartBlock AddKey*)* Finish
class FilterBlockBuilder {
 public:
  explicit FilterBlockBuilder(const FilterPolicy*);

  FilterBlockBuilder(const FilterPolicy&) = delete;
  FilterBlockBuilder& operator=(const FilterBlockBuilder&) = delete;

  void StartBlock(uint64_t block_offset);
  void AddKey(const Slice& key);
  Slice Finish();

 private:
  void GenerateFilter();
  const FilterPolicy* policy_;  // 无存储数据，只提供方法
  std::string keys_;            // 扁平化的键内容
  std::vector<size_t> start_;   // 键索引
  std::string result_;  // 到目前为止计算的过滤器数据，最终写入磁盘的数据
  std::vector<Slice> tmp_keys_;  // policy_->CreateFilter() argument
  std::vector<uint32_t> filter_offsets_;
};

class FilterBlockReader {
 public:
  FilterBlockReader(const FilterPolicy* policy, const Slice& contents);
  bool KeyMayMatch(uint64_t block_offset, const Slice& key);

 private:
  const FilterPolicy* policy_;
  const char* data_;  // filterdata起始位置
  // [offset of beginning of offset array] : 4 bytes的起始位置
  const char* offset_;
  size_t num_;
  size_t base_lg_;  // lg(base) : 1 byte
};

}  // namespace leveldb

#endif