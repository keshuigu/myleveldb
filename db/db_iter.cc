#include "db/db_iter.h"

#include "db/db_impl.h"
#include "db/dbformat.h"
#include "db/filename.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"
#include "port/port.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/random.h"

namespace leveldb {

namespace {
// 构成数据库表示的内存表和 sstables 包含 (userkey,seq,type) => uservalue 条目。
// DBIter 将在数据库表示中找到的同一 userkey
// 的多个条目合并为一个条目，同时考虑序列号、删除标记、覆盖等。
class DBIter : public Iterator {
 public:
  // 迭代器当前移动的方向是哪个？
  // (1) 当向前移动时，内部迭代器定位在生成 this->key(), this->value()
  // 的确切条目。 (2) 当向后移动时，内部迭代器定位在所有用户键等于 this->key()
  // 的条目之前。
  enum Direction { kForward, kReverse };

  DBIter(DBImpl* db, const Comparator* cmp, Iterator* iter, SequenceNumber s,
         uint32_t seed)
      : db_(db),
        user_comparator_(cmp),
        iter_(iter),
        sequence_(s),
        direction_(kForward),
        valid_(false),
        rnd_(seed),
        bytes_until_read_sampling_(RandomCompactionPeriod()) {}

  DBIter(const DBIter&) = delete;
  DBIter& operator=(const DBIter&) = delete;

  ~DBIter() override { delete iter_; }

  bool Valid() const override { return valid_; }
  Slice key() const override {
    assert(valid_);
    return (direction_ == kForward) ? ExtractUserKey(iter_->key()) : saved_key_;
  }
  Slice value() const override {
    assert(valid_);
    return (direction_ == kForward) ? iter_->value() : saved_value_;
  }
  Status status() const override {
    if (status_.ok()) {
      return iter_->status();
    } else {
      return status_;
    }
  }

  void Next() override;
  void Prev() override;
  void Seek(const Slice& target) override;
  void SeekToFirst() override;
  void SeekToLast() override;

 private:
  void FindNextUserEntry(bool skipping, std::string* skip);
  void FindPrevUserEntry();
  bool ParseKey(ParsedInternalKey* key);

  inline void SaveKey(const Slice& k, std::string* dst) {
    dst->assign(k.data(), k.size());
  }

  inline void ClearSavedValue() {
    // 内存超过1MB，释放内存
    if (saved_value_.capacity() > 1048576) {
      std::string empty;
      std::swap(empty, saved_value_);
    } else {
      saved_value_.clear();
    }
  }

  // 选择在安排压缩之前可以读取的字节数。
  size_t RandomCompactionPeriod() {
    return rnd_.Uniform(2 * config::kReadBytesPeriod);
  }

  DBImpl* db_;
  const Comparator* const user_comparator_;
  Iterator* const iter_;
  SequenceNumber const sequence_;
  Status status_;
  std::string saved_key_;    // 当 direction_==kReverse 时，等于当前键
  std::string saved_value_;  // 当 direction_==kReverse 时，等于当前原始值
  Direction direction_;
  bool valid_;
  Random rnd_;
  size_t bytes_until_read_sampling_;
};

inline bool DBIter::ParseKey(ParsedInternalKey* ikey) {
  Slice k = iter_->key();
  size_t bytes_read = k.size() + iter_->value().size();
  while (bytes_until_read_sampling_ < bytes_read) {
    bytes_until_read_sampling_ += RandomCompactionPeriod();
    db_->RecordReadSample(k);
  }
  // 确保采样时读取的字节数大于bytes_read
  assert(bytes_until_read_sampling_ >= bytes_read);
  bytes_until_read_sampling_ -= bytes_read;
  if (!ParseInternalKey(k, ikey)) {
    status_ = Status::Corruption("corrupted internal key in DBIter");
    return false;
  } else {
    return true;
  }
}

void DBIter::Next() {
  assert(valid_);
  if (direction_ = kReverse) {
    // iter_ 指向 this->key() 条目之前，
    // 因此进入 this->key() 条目的范围，然后使用下面的正常跳过代码。
    direction_ = kForward;
    if (!iter_->Valid()) {
      iter_->SeekToFirst();
    } else {
      iter_->Next();
    }
    if (!iter_->Valid()) {
      valid_ = false;
      saved_key_.clear();
      return;
    }
    // saved_key_ 已经包含要跳过的键。
  } else {
    // 将当前键存储在 saved_key_ 中，以便在下面跳过它。
    SaveKey(ExtractUserKey(iter_->key()), &saved_key_);
    // iter_ 指向当前键。我们现在可以安全地移动到下一个键，以避免检查当前键。
    iter_->Next();
    if (!iter_->Valid()) {
      valid_ = false;
      saved_key_.clear();
      return;
    }
  }
  FindNextUserEntry(true, &saved_key_);
}

void DBIter::FindNextUserEntry(bool skipping, std::string* skip) {
  // 循环直到找到一个可接受的条目
  assert(iter_->Valid());
  assert(direction_ == kForward);
  do {
    ParsedInternalKey ikey;
    // leveldb中键序列号降序排列
    // 因此扫描到的第一个序列号为最新的操作
    if (ParseKey(&ikey) && ikey.sequence <= sequence_) {
      switch (ikey.type) {
        case kTypeDeletion:
          // 安排跳过此键的所有即将到来的条目，因为它们被此删除隐藏。
          SaveKey(ikey.user_key, skip);
          skipping = true;
          break;
        case kTypeValue:
          if (skipping &&
              user_comparator_->Compare(ikey.user_key, *skip) <= 0) {
            // 此键已被隐藏
          } else {
            valid_ = true;
            saved_key_.clear();
            return;
          }
          break;
      }
    }
    iter_->Next();
  } while (iter_->Valid());
  saved_value_.clear();
  valid_ = false;
}
void DBIter::Prev() {
  assert(valid_);
  if (direction_ == kForward) {
    // iter_指向当前条目。向后扫描直到键发生变化，以便我们可以使用正常的反向扫描代码。
    assert(iter_->Valid());
    SaveKey(ExtractUserKey(iter_->key()),
            &saved_key_);  // 保存的是当前的条目，还不是目标条目
    while (true) {
      iter_->Prev();
      if (!iter_->Valid()) {
        valid_ = false;
        saved_key_.clear();
        ClearSavedValue();
        return;
      }
      if (user_comparator_->Compare(ExtractUserKey(iter_->key()), saved_key_) <
          0) {
        break;
      }
    }
    direction_ = kReverse;
  }
  FindPrevUserEntry();
}

void DBIter::FindPrevUserEntry() {
  assert(direction_ == kReverse);
  ValueType value_type = kTypeDeletion;
  if (iter_->Valid()) {
    do {
      ParsedInternalKey ikey;
      if (ParseKey(&ikey) && ikey.sequence <= sequence_) {
        if ((value_type != kTypeDeletion) &&
            user_comparator_->Compare(ikey.user_key, saved_key_) < 0) {
          break;
        }
        value_type = ikey.type;
        // 如果删除，当前键清空，下个键必定不进入上面的if
        if (value_type == kTypeDeletion) {
          saved_key_.clear();
          ClearSavedValue();
        } else {
          Slice raw_value = iter_->value();
          if (saved_value_.capacity() > raw_value.size() + 1048576) {
            std::string empty;
            std::swap(empty, saved_value_);
          }
          SaveKey(ExtractUserKey(iter_->key()), &saved_key_);
          saved_value_.assign(raw_value.data(), raw_value.size());
        }
      }
      iter_->Prev();
    } while (iter_->Valid());
  }
  if (value_type == kTypeDeletion) {
    // End
    valid_ = false;
    saved_key_.clear();
    ClearSavedValue();
    direction_ = kForward;
  } else {
    valid_ = true;
  }
}

void DBIter::Seek(const Slice& target) {
  direction_ = kForward;
  ClearSavedValue();
  saved_key_.clear();
  AppendInternalKey(&saved_key_,
                    ParsedInternalKey(target, sequence_, kValueTypeForSeek));
  iter_->Seek(saved_key_);
  if (iter_->Valid()) {
    FindNextUserEntry(false, &saved_key_ /* temporary storage */);
  } else {
    valid_ = false;
  }
}

void DBIter::SeekToFirst() {
  direction_ = kForward;
  ClearSavedValue();
  iter_->SeekToFirst();
  if (iter_->Valid()) {
    FindNextUserEntry(false, &saved_key_ /* temporary storage */);
  } else {
    valid_ = false;
  }
}

void DBIter::SeekToLast() {
  direction_ = kReverse;
  ClearSavedValue();
  iter_->SeekToLast();
  FindPrevUserEntry();
}
}  // namespace
Iterator* NewDBIterator(DBImpl* db, const Comparator* user_key_comparator,
                        Iterator* internal_iter, SequenceNumber sequence,
                        uint32_t seed) {
  return new DBIter(db, user_key_comparator, internal_iter, sequence, seed);
}
}  // namespace leveldb
