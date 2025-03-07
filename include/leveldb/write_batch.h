// WriteBatch 保存了一组要原子性地应用到数据库的更新。
//
// 更新按添加到 WriteBatch 的顺序应用。例如，在写入以下批处理后，"key" 的值将是
// "v3"：
//
//    batch.Put("key", "v1");
//    batch.Delete("key");
//    batch.Put("key", "v2");
//    batch.Put("key", "v3");
//
// 多个线程可以在没有外部同步的情况下调用 WriteBatch 的 const 方法，
// 但如果任何线程可能调用非 const 方法，则访问同一 WriteBatch
// 的所有线程必须使用外部同步。

#ifndef STORAGE_LEVELDB_INCLUDE_WRITE_BATCH_H_
#define STORAGE_LEVELDB_INCLUDE_WRITE_BATCH_H_

#include <string>

#include "leveldb/export.h"
#include "leveldb/status.h"

namespace leveldb {
class Slice;
class LEVELDB_EXPORT WriteBatch {
 public:
  class LEVELDB_EXPORT Handler {
   public:
    virtual ~Handler();
    virtual void Put(const Slice& key, const Slice& value) = 0;
    virtual void Delete(const Slice& key) = 0;
  };

  WriteBatch();

  WriteBatch(const WriteBatch&) = default;
  WriteBatch& operator=(const WriteBatch&) = default;
  ~WriteBatch();

  void Put(const Slice& key, const Slice& value);

  void Delete(const Slice& key);

  void Clear();
  // 由此批处理引起的数据库大小变化。
  //
  // 这个数字与实现细节相关，可能会在不同版本中发生变化。它用于 LevelDB
  // 的使用指标。
  size_t ApproximateSize() const;

  // 将 "source" 中的操作复制到此批处理中。
  //
  // 这在 O(source size) 时间内运行。然而，其常数因子比使用 Handler 调用
  // Iterate() 复制操作到此批处理要好。
  void Append(const WriteBatch& source);

  // Support for iterating over the contents of a batch.
  Status Iterate(Handler* handler) const;

 private:
  friend class WriteBatchInternal;

  std::string rep_;  // See comment in write_batch.cc for the format of rep_
};
}  // namespace leveldb

#endif