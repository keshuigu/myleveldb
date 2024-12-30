
#include "db/log_reader.h"

#include <cstdio>

#include "leveldb/env.h"
#include "util/coding.h"
#include "util/crc32c.h"

namespace leveldb {
namespace log {
Reader::Reporter::~Reporter() = default;

Reader::Reader(SequentialFile* file, Reporter* reporter, bool checksum,
               uint64_t initial_offset)
    : file_(file),
      reporter_(reporter),
      checksum_(checksum),
      backing_store_(new char[kBlockSize]),
      buffer_(),
      eof_(false),
      last_record_offset_(0),
      end_of_buffer_offset_(0),
      initial_offset_(initial_offset),
      resyncing_(initial_offset > 0) {}

Reader::~Reader() { delete[] backing_store_; }

bool Reader::SkipToInitialBlock() {
  const size_t offset_in_block = initial_offset_ % kBlockSize;
  uint64_t block_start_location = initial_offset_ - offset_in_block;
  // 尾部不检查
  if (offset_in_block > kBlockSize - 6) {
    block_start_location += kBlockSize;
  }
  end_of_buffer_offset_ = block_start_location;

  // 跳转到当其块
  if (block_start_location > 0) {
    Status skip_status = file_->Skip(block_start_location);
    if (!skip_status.ok()) {
      ReportDrop(block_start_location, skip_status);
      return false;
    }
  }
  return true;
}

bool Reader::ReadRecord(Slice* record, std::string* scratch) {
  if (last_record_offset_ < initial_offset_) {
    if (!SkipToInitialBlock()) {
      return false;
    }
  }

  scratch->clear();
  record->clear();

  bool in_fragmented_record = false;
  // 记录我们正在读取的逻辑记录的偏移量
  // 0 是一个虚拟值，用于make compilers happy
  uint64_t prospective_record_offset = 0;

  Slice fragment;
  while (true) {
    const uint32_t record_type = ReadPhysicalRecord(&fragment);

    // ReadPhysicalRecord 可能在其内部缓冲区中只剩下一个空的尾部。
    // 现在它已经返回，计算下一个物理记录的偏移量，正确考虑其头部大小。

    // 该记录的开始位置
    uint64_t physical_record_offset =
        end_of_buffer_offset_ - buffer_.size() - kHeaderSize - fragment.size();
    if (resyncing_) {
      if (record_type == kMiddleType) {
        continue;
      } else if (record_type == kLastType) {
        resyncing_ = false;
        continue;
      } else {
        resyncing_ = false;
      }
    }

    switch (record_type) {
      case kFullType:
        if (in_fragmented_record) {
          // 处理早期版本的 log::Writer 中的错误
          // 它可能会在块的尾部发出一个空的 kFirstType 记录，
          // 然后在下一个块的开头发出一个 kFullType 或 kFirstType 记录。
          if (!scratch->empty()) {
            ReportCorruption(scratch->size(), "partial record without end(1)");
          }
        }
        prospective_record_offset = physical_record_offset;
        scratch->clear();
        *record = fragment;
        last_record_offset_ = prospective_record_offset;
        return true;

      case kFirstType:
        if (in_fragmented_record) {
          // 处理早期版本的 log::Writer 中的错误
          // 它可能会在块的尾部发出一个空的 kFirstType 记录，
          // 然后在下一个块的开头发出一个 kFullType 或 kFirstType 记录。
          if (!scratch->empty()) {
            ReportCorruption(scratch->size(), "partial record without end(2)");
          }
        }
        prospective_record_offset = physical_record_offset;
        scratch->assign(fragment.data(), fragment.size());
        in_fragmented_record = true;
        break;
      case kMiddleType:
        if (!in_fragmented_record) {
          ReportCorruption(fragment.size(),
                           "missing start of fragmented record(1)");
        } else {
          scratch->append(fragment.data(), fragment.size());
        }
        break;
      case kLastType:
        if (!in_fragmented_record) {
          ReportCorruption(fragment.size(),
                           "missing start of fragmented record(2)");
        } else {
          scratch->append(fragment.data(), fragment.size());
          *record = Slice(*scratch);
          last_record_offset_ = prospective_record_offset;
          return true;
        }
        break;
      case kEof:
        if (in_fragmented_record) {
          scratch->clear();
        }
        return false;
      case kBadRecord:
        if (in_fragmented_record) {
          ReportCorruption(scratch->size(), "error in middle of record");
          in_fragmented_record = false;
          scratch->clear();
        }
        break;
      default:
        char buf[40];
        std::snprintf(buf, sizeof(buf), "unknown record type %u", record_type);
        ReportCorruption(
            (fragment.size() + (in_fragmented_record ? scratch->size() : 0)),
            buf);
        in_fragmented_record = false;
        scratch->clear();
        break;
    }
  }
  return false;
}

uint64_t Reader::LastRecordOffset() { return last_record_offset_; }

void Reader::ReportCorruption(uint64_t bytes, const char* reason) {
  ReportDrop(bytes, Status::Corruption(reason));
}
void Reader::ReportDrop(uint64_t bytes, const Status& reason) {
  if (reporter_ != nullptr &&
      end_of_buffer_offset_ - buffer_.size() - bytes >= initial_offset_) {
    reporter_->Corruption(static_cast<size_t>(bytes), reason);
  }
}

unsigned int Reader::ReadPhysicalRecord(Slice* result) {
  // 只读一个record
  while (true) {
    // 1 最开始buffer_.size() = 0 读一个块
    if (buffer_.size() < kHeaderSize) {
      // eof_指明了是否完整读取了一个块
      if (!eof_) {
        // Last read was a full read, so this is a trailer to skip
        // ?
        buffer_.clear();
        // 读一个块
        Status status = file_->Read(kBlockSize, &buffer_, backing_store_);
        end_of_buffer_offset_ += buffer_.size();
        if (!status.ok()) {
          buffer_.clear();
          ReportDrop(kBlockSize, status);
          eof_ = true;
          return kEof;
        } else if (buffer_.size() < kBlockSize) {
          eof_ = true;
        }
        continue;  // while (true)
      } else {
        // 请注意，如果 buffer_ 非空，我们在文件末尾有一个截断的头部，
        // 这可能是由于写入器在写入头部的过程中崩溃导致的。
        // 不要将此视为错误，只需报告 EOF。
        buffer_.clear();
        return kEof;
      }
    }
    // buffer存在内容，进行解析
    const char* header = buffer_.data();
    const uint32_t a = static_cast<uint32_t>(header[4]) & 0xff;
    const uint32_t b = static_cast<uint32_t>(header[5]) & 0xff;
    const unsigned int type = header[6];
    const uint32_t length = a | (b << 8);

    if (kHeaderSize + length > buffer_.size()) {
      size_t drop_size = buffer_.size();
      buffer_.clear();
      if (!eof_) {
        ReportCorruption(drop_size, "bad record length");
        return kBadRecord;
      }
      // 如果在未读取到 |length| 字节的有效负载时已到达文件末尾，
      // 假设写入器在写入记录的过程中崩溃。
      // 不要报告损坏。
      return kEof;
    }

    if (type == kZeroType && length == 0) {
      // 跳过零长度记录而不报告任何丢弃，因为这些记录是由 env_posix.cc 中基于
      // mmap 的写入代码生成的，该代码预分配文件区域。
      buffer_.clear();
      return kBadRecord;
    }

    if (checksum_) {
      uint32_t expected_crc = crc32c::Unmask(DecodeFixed32(header));
      uint32_t actual_crc = crc32c::Value(header + 6, 1 + length);
      if (actual_crc != expected_crc) {
        // 丢弃缓冲区的其余部分，因为 "length" 本身可能已损坏，
        // 如果我们信任它，我们可能会找到一些看起来像有效日志记录的真实日志记录片段。
        size_t drop_size = buffer_.size();
        buffer_.clear();
        ReportCorruption(drop_size, "checksum mismatch");
        return kBadRecord;
      }
    }

    buffer_.remove_prefix(kHeaderSize + length);
    // end_of_buffer_offset_ 是整个文件的偏移量，并且只会递增
    // - buffer_.size() - kHeaderSize - length 事实上就是remove_prefix前的长度
    // 该值也就是当前块的起始位置
    if (end_of_buffer_offset_ - buffer_.size() - kHeaderSize - length <
        initial_offset_) {
      result->clear();
      return kBadRecord;
    }
    *result = Slice(header + kHeaderSize, length);
    return type;
  }
}
}  // namespace log

}  // namespace leveldb
