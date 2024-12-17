#ifndef STORAGE_LEVELDB_INCLUDE_TABLE_H_
#define STORAGE_LEVELDB_INCLUDE_TABLE_H_

#include <cstdint>

#include "leveldb/export.h"
#include "leveldb/iterator.h"

namespace leveldb {
class Block;
class BlockHandle;
class Footer;
struct Options;
class RandomAccessFile;
struct ReadOptions;
class TableCache;

// Table 是一个从字符串到字符串的有序映射。Table 是不可变且持久的。Table
// 可以在没有外部同步的情况下安全地被多个线程访问。
class LEVELDB_EXPORT Table {
 public:
  // 尝试打开存储在 "file" 的字节 [0..file_size)
  // 中的表，并读取必要的元数据条目以允许从表中检索数据。 如果成功，返回 ok 并将
  // "*table" 设置为新打开的表。当不再需要时，客户端应删除 "*table"。
  // 如果在初始化表时发生错误，将 "*table" 设置为 nullptr 并返回非 ok
  // 状态。不接管 "*source" 的所有权，但客户端必须确保 "source"
  // 在返回的表的生命周期内保持有效。 *file 在此 Table 使用期间必须保持有效。
  static Status Open(const Options& options, RandomAccessFile* file,
                     uint64_t file_size, Table** table);
  Table(const Table&) = delete;
  Table& operator=(const Table&) = delete;

  ~Table();

  // 返回一个新的迭代器，用于遍历表的内容。
  // NewIterator()
  // 的结果最初是无效的（调用者必须在使用迭代器之前调用其中一个Seek 方法）。
  Iterator* NewIterator(const ReadOptions&) const;

  // 给定一个键，返回文件中该键的数据开始（或如果键存在于文件中将开始）的近似字节偏移量。返回值以文件字节为单位，因此包括底层数据压缩等影响。
  // 例如，表中最后一个键的近似偏移量将接近文件长度。
  uint64_t ApproximateOffsetOf(const Slice& key) const;

 private:
  friend class TableCache;
  struct Rep;

  static Iterator* BlockReader(void*, const ReadOptions&, const Slice&);

  explicit Table(Rep* rep) : rep_(rep) {}
  // 在调用 Seek(key) 后，使用找到的条目调用 (*handle_result)(arg, ...)
  // 如果过滤策略表明键不存在，则可能不会进行此调用。
  Status InternalGet(const ReadOptions&, const Slice& key, void* arg,
                     void (*handle_result)(void* arg, const Slice& k,
                                           const Slice& v));

  void ReadMeta(const Footer& footer);
  void ReadFilter(const Slice& filter_handle_value);

  Rep* const rep_;
};

}  // namespace leveldb

#endif