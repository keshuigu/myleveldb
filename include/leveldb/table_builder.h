#ifndef STORAGE_LEVELDB_INCLUDE_TABLE_BUILDER_H_
#define STORAGE_LEVELDB_INCLUDE_TABLE_BUILDER_H_

#include <cstdint>

#include "leveldb/export.h"
#include "leveldb/options.h"
#include "leveldb/status.h"

namespace leveldb {
class BlockBuilder;
class BlockHandle;
class WritableFile;

class LEVELDB_EXPORT TableBuilder {
 public:
  // 创建一个构建器，该构建器会将正在构建的表的内容存储在
  // *file中。不会关闭文件。调用者需要在调用 Finish() 之后关闭文件
  TableBuilder(const Options& options, WritableFile* file);
  TableBuilder(const TableBuilder&) = delete;
  TableBuilder& operator=(const TableBuilder&) = delete;

  // REQUIRES: Either Finish() or Abandon() has been called.
  ~TableBuilder();
  // 更改此构建器使用的选项。注意：只有部分选项字段可以在构建后更改。如果某个字段不允许动态更改，并且传递给构造函数的结构中的值与传递给此方法的结构中的值不同，则此方法将返回错误而不更改任何字段。
  Status ChangeOptions(const Options& options);

  // 向正在构建的表中添加键和值。
  // 要求：根据比较器，键必须在任何先前添加的键之后。
  // 要求：未调用 Finish() 或 Abandon()
  void Add(const Slice& key, const Slice& value);

  // 高级操作：将任何缓冲的键/值对刷新到文件中。
  // 可用于确保两个相邻的条目永远不会存在于同一个数据块中。大多数客户端不需要使用此方法。
  // 要求：未调用 Finish() 或 Abandon()。
  void Flush();

  Status status() const;

  Status Finish();

  // 表示应放弃此构建器的内容。在此函数返回后停止使用传递给构造函数的文件。
  // 如果调用者不打算调用 Finish()，则必须在销毁此构建器之前调用 Abandon()。
  // 要求：未调用 Finish() 或 Abandon()。
  void Abandon();

  // 调用add的次数
  uint64_t NumEntries() const;

  // 目前为止生成的文件大小。如果在成功调用 Finish()之后调用，则返回最终生成的文件大小。
  uint64_t FileSize() const;

 private:
  bool ok() const { return status().ok(); }
  void WriteBlock(BlockBuilder* block, BlockHandle* handle);
  void WriteRawBlock(const Slice& data, CompressionType, BlockHandle* handle);

  struct Rep;
  Rep* rep_;
};

}  // namespace leveldb

#endif