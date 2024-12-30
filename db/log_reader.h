#ifndef STORAGE_LEVELDB_DB_LOG_READER_H_
#define STORAGE_LEVELDB_DB_LOG_READER_H_

#include <cstdint>

#include "db/log_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {
class SequentialFile;
namespace log {

class Reader {
 public:
  // Interface for reporting errors.
  class Reporter {
   public:
    virtual ~Reporter();

    // 检测到一些损坏。"bytes" 是由于损坏而丢弃的大致字节数。
    virtual void Corruption(size_t bytes, const Status& status) = 0;
  };

  // 创建一个从 "*file" 返回日志记录的读取器。
  // 在此 Reader 使用期间，"*file" 必须保持有效。
  //
  // 如果 "reporter" 非空，则在检测到损坏导致某些数据被丢弃时通知它。
  // 在此 Reader 使用期间，"*reporter" 必须保持有效。
  //
  // 如果 "checksum" 为 true，则在可用时验证校验和。
  //
  // Reader 将从文件中物理位置 >= initial_offset 的第一个记录开始读取。
  Reader(SequentialFile* file, Reporter* reporter, bool checksum,
         uint64_t initial_offfset);

  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

  ~Reader();

  // 将下一条记录读取到*record中。如果读取成功则返回true，如果到达输入末尾则返回false。
  // 可能会使用"*scratch"作为临时存储。
  // 填充到*record中的内容仅在此读取器上的下一个变更操作或对*scratch的下一个变更之前有效。
  bool ReadRecord(Slice* record, std::string* scratch);

  // 返回 ReadRecord 返回的最后一条记录的物理偏移量。
  //
  // 在第一次调用 ReadRecord 之前未定义。
  uint64_t LastRecordOffset();

 private:
  // Extend record types with the following special values
  enum {
    kEof = kMaxRecordType + 1,
    // 当我们发现无效的物理记录时返回。
    // 目前有三种情况会发生这种情况：
    // * 记录的 CRC 无效（ReadPhysicalRecord 报告丢弃）
    // * 记录是 0 长度记录（不报告丢弃）
    // * 记录低于构造函数的 initial_offset（不报告丢弃）
    kBadRecord = kMaxRecordType + 2
  };
  // 跳过所有完全在 "initial_offset_" 之前的块。
  //
  // 成功时返回 true。处理报告。
  bool SkipToInitialBlock();

  // 返回类型，或前面特殊值之一
  unsigned int ReadPhysicalRecord(Slice* result);

  // 向报告器报告丢弃的字节。
  // 在调用之前必须更新 buffer_ 以移除丢弃的字节。
  void ReportCorruption(uint64_t bytes, const char* reason);

  void ReportDrop(uint64_t bytes, const Status& reason);

  SequentialFile* const file_;
  Reporter* const reporter_;
  bool const checksum_;
  char* const backing_store_;
  Slice buffer_;
  bool eof_;  // Last Read() indicated EOF by returning < kBlockSize

  uint64_t last_record_offset_;

  uint64_t end_of_buffer_offset_;

  uint64_t const initial_offset_;

  // 如果在 seek 之后重新同步（initial_offset_ > 0），则为 true。
  // 特别是在这种模式下，可以静默跳过一系列 kMiddleType 和 kLastType 记录。
  bool resyncing_;
};

}  // namespace log

}  // namespace leveldb

#endif