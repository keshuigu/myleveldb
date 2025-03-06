#ifndef STORAGE_LEVELDB_DB_SNAPSHOT_H_
#define STORAGE_LEVELDB_DB_SNAPSHOT_H_

#include "db/dbformat.h"
#include "leveldb/db.h"

namespace leveldb {
class SnapshotList;
class SnapshotImpl : public Snapshot {
 public:
  SnapshotImpl(SequenceNumber sequence_number)
      : sequence_number_(sequence_number) {}

  SequenceNumber sequence_number() const { return sequence_number_; }

 private:
  friend class SnapshotList;

  SnapshotImpl* prev_;
  SnapshotImpl* next_;

  const SequenceNumber sequence_number_;

#if !defined(NDEBUG)
  SnapshotList* list_ = nullptr;
#endif
};

class SnapshotList {
 public:
  SnapshotList() : head_(0) {
    head_.prev_ = &head_;
    head_.next_ = &head_;
  }

  bool empty() const { return head_.next_ == &head_; }
  SnapshotImpl* oldest() const {
    assert(!empty());
    return head_.prev_;
  }
  SnapshotImpl* newset() const {
    assert(!empty());
    return head_.prev_;
  }

  SnapshotImpl* New(SequenceNumber sequence_number) {
    assert(empty() || newset()->sequence_number_ <= sequence_number);
    SnapshotImpl* snapshot = new SnapshotImpl(sequence_number);
#if !defined(NDEBUG)
    snapshot->list_ = this;
#endif
    snapshot->next_ = &head_;
    snapshot->prev_ = head_.prev_;
    head_.prev_->next_ = snapshot;
    head_.prev_ = snapshot;
    return snapshot;
  }

  // 从此列表中移除一个 SnapshotImpl。
  //
  // 该快照必须通过在此列表上调用 New() 创建。
  //
  // 快照指针不应为 const，因为它的内存会被释放。
  // 但是，这将迫使我们更改 DB::ReleaseSnapshot()，
  // 该函数在 API 中，目前接受一个 const Snapshot。
  // TODO
  void Delete(const SnapshotImpl* snapshot) {
#if !defined(NDEBUG)
    assert(snapshot->list_ == this);
#endif
    snapshot->prev_->next_ = snapshot->next_;
    snapshot->next_->prev_ = snapshot->prev_;
    delete snapshot;
  }

 private:
  SnapshotImpl head_;
};

}  // namespace leveldb

#endif
