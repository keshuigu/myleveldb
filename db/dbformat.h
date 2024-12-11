#ifndef STORAGE_LEVELDB_DB_DBFORMAT_H_
#define STORAGE_LEVELDB_DB_DBFORMAT_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "leveldb/slice.h"
#include "leveldb/table_builder.h"
#include "util/coding.h"
#include "util/logging.h"

namespace leveldb {

namespace config {
static const int kNumLevels = 7;

// 触发L0压缩的文件数
static const int kL0_CompactionTrigger = 4;

// Level-0 文件数量的软限制。达到8个文件我们会减慢写入速度。
static const int kL0_SlowdownWritesTrigger = 8;

// Level-0 文件的最大数量。达到12个文件我们会停止写入。
static const int kL0_StopWritesTrigger = 12;

// 如果新的压缩 memtable 不会产生重叠，它将被推送到的最大层级。
// 我们尝试推送到第2层，以避免相对昂贵的0层到1层的压缩操作，并避免一些昂贵的清单文件操作。
// 我们不会推送到最大的层级，因为如果同一个键空间被反复覆盖，这会产生大量浪费的磁盘空间。
static const int kMaxMemCompactLevel = 2;

// 在迭代期间读取的数据样本之间的近似字节间隔。
static const int kReadBytesPeriod = 1048576;

}  // namespace config

class InternalKey;

// 不可修改 硬编码
enum ValueType { kTypeDeletion = 0x0, kTypeValue = 0x1 };

// kValueTypeForSeek定义了在构造ParsedInternalKey对象以查找特定序列号时应传递的ValueType
// 因为我们按降序排序序列号，并且值类型作为低8位嵌入在内部键的序列号中，
// 所以我们需要使用编号最高的ValueType，而不是编号最低的
static const ValueType kValueTypeForSeek = kTypeValue;

typedef uint64_t SequenceNumber;

// 低8位留给ValueType
static const SequenceNumber kMaxSequenceNumber = ((0x1ull << 56) - 1);

struct ParsedInternalKey {
  Slice user_key;
  SequenceNumber sequence;
  ValueType type;

  ParsedInternalKey() {}
  ParsedInternalKey(const Slice& u, const SequenceNumber& seq, ValueType t)
      : user_key(u), sequence(seq), type(t) {}
  std::string DebugString() const;
};

inline size_t InternalKeyEncodingLength(const ParsedInternalKey& key) {
  return key.user_key.size() + 8;
}

void AppendInternalKey(std::string* result, const ParsedInternalKey& key);

bool ParseInternalKey(const Slice& internal_key, ParsedInternalKey* result);

inline Slice ExtractUserKey(const Slice& internal_key) {
  assert(internal_key.size() >= 8);
  return Slice(internal_key.data(), internal_key.size() - 8);
}

// 一个用于内部键的比较器，它使用指定的比较器来比较用户键部分，并通过降序的序列号来打破平局。
class InternalKeyComparator : public Comparator {
 public:
  explicit InternalKeyComparator(const Comparator* c) : user_comparator_(c) {}
  const char* Name() const override;
  int Compare(const Slice& a, const Slice& b) const override;
  void FindShortestSeparator(std::string* start,
                             const Slice& limit) const override;
  void FindShortSuccessor(std::string* key) const override;

  const Comparator* user_comparator() const { return user_comparator_; }

  int Compare(const InternalKey& a, const InternalKey& b) const;

 private:
  const Comparator* user_comparator_;
};

class InternalFilterPolicy : public FilterPolicy {
 public:
  explicit InternalFilterPolicy(const FilterPolicy* p) : user_policy_(p) {}
  const char* Name() const override;
  void CreateFilter(const Slice* keys, int n, std::string* dst) const override;
  bool KeyMayMatch(const Slice& key, const Slice& filter) const override;

 private:
  const FilterPolicy* const user_policy_;
};

class InternalKey {
 public:
  InternalKey() {}
  InternalKey(const Slice& user_key, SequenceNumber s, ValueType t) {
    AppendInternalKey(&rep_, ParsedInternalKey(user_key, s, t));
  }

  bool DecodeFrom(const Slice& s) {
    rep_.assign(s.data(), s.size());
    return !rep_.empty();
  }

  Slice Encode() const {
    assert(!rep_.empty());
    // return Slice(rep_);
    return rep_;  // 隐式转换
  }

  Slice user_key() const { return ExtractUserKey(rep_); }

  void SetFrom(const ParsedInternalKey& p) {
    rep_.clear();
    AppendInternalKey(&rep_, p);
  }

  void Clear() { rep_.clear(); }

  std::string DebugString() const;

 private:
  std::string rep_;
};

inline int InternalKeyComparator::Compare(const InternalKey& a,
                                          const InternalKey& b) const {
  return Compare(a.Encode(), b.Encode());
}

inline bool ParseInternalKey(const Slice& internal_key,
                             ParsedInternalKey* result) {
  const size_t n = internal_key.size();
  if (n < 8) {
    return false;
  }
  // seq + type
  uint64_t num = DecodeFixed64(internal_key.data() + n - 8);
  uint8_t c = num & 0xff;
  result->sequence = num >> 8;
  result->type = static_cast<ValueType>(c);
  result->user_key = Slice(internal_key.data(), n - 8);
  return (c <= static_cast<uint8_t>(kTypeValue));
}

class LookupKey {
 public:
  LookupKey(const Slice& user_key, SequenceNumber sequence);

  LookupKey(const LookupKey&) = delete;
  LookupKey& operator=(const LookupKey&) = delete;
  ~LookupKey();

  // with length
  Slice memtable_key() const { return Slice(start_, end_ - start_); }

  Slice internal_key() const { return Slice(kstart_, end_ - kstart_); }

  Slice user_key() const { return Slice(kstart_, end_ - kstart_ - 8); }

 private:
  // 我们构造一个如下形式的字符数组：
  // klength varint32      <-- start_
  // userkey char[klength] <-- kstart_
  // tag uint64
  //                       <-- end_
  // 该数组是一个合适的MemTable键。
  // 从 "userkey" 开始的后缀可以用作 InternalKey
  const char* start_;
  const char* kstart_;
  const char* end_;
  char space_[200];
};

inline LookupKey::~LookupKey() {
  if (start_ != space_) {
    delete[] start_;
  }
}

}  // namespace leveldb

#endif