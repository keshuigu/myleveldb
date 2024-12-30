#ifndef STORAGE_LEVELDB_DB_LOG_WRITER_H_
#define STORAGE_LEVELDB_DB_LOG_WRITER_H_

#include <cstdint>

#include "db/log_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {
class WritableFile;
namespace log {
class Writer {
 public:
  // 创建一个将数据追加到 "*dest" 的写入器。
  // "*dest" 必须最初为空。
  // 在此 Writer 使用期间，"*dest" 必须保持有效。
  explicit Writer(WritableFile* dest);

  // 创建一个将数据追加到 "*dest" 的写入器。
  // "*dest" 必须具有初始长度 "dest_length"。
  // 在此 Writer 使用期间，"*dest" 必须保持有效。
  Writer(WritableFile* dest, uint64_t dest_length);

  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;

  ~Writer();

  Status AddRecord(const Slice& slice);

 private:
  Status EmitPhysicalRecord(RecordType type, const char* ptr, size_t length);

  WritableFile* dest_;
  int block_offset_;  // Current offset in block

  // 所有支持的记录类型的crc32c值。
  // 这些值是预先计算的，以减少计算存储在头部的记录类型的 crc 的开销。
  uint32_t type_crc_[kMaxRecordType + 1];
};
}  // namespace log

}  // namespace leveldb

#endif