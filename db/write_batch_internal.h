#ifndef STORAGE_LEVELDB_DB_WRITE_BATCH_INTERNAL_H_
#define STORAGE_LEVELDB_DB_WRITE_BATCH_INTERNAL_H_

#include "db/dbformat.h"
#include "leveldb/write_batch.h"

namespace leveldb {
class MemTable;
// WriteBatchInternal 提供了一些静态方法，用于操作我们不希望出现在公共
// WriteBatch 接口中的 WriteBatch。
class WriteBatchInternal {
 public:
  // 返回batch中条目数
  static int Count(const WriteBatch* batch);
  // 设置批处理中条目的数量
  static void SetCount(WriteBatch* batch, int n);
  // 返回此批处理开始的序列号。
  static SequenceNumber Sequence(const WriteBatch* batch);

  // 将指定的数字存储为此批处理开始的序列号。
  static void SetSequence(WriteBatch* batch, SequenceNumber seq);

  static Slice Contents(const WriteBatch* batch) { return Slice(batch->rep_); }

  static size_t ByteSize(const WriteBatch* batch) { return batch->rep_.size(); }

  static void SetContents(WriteBatch* b, const Slice& contents);

  static Status InsertInto(const WriteBatch* batch, MemTable* memtable);

  static void Append(WriteBatch* dst, const WriteBatch* src);
};
}  // namespace leveldb

#endif