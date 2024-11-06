#include "leveldb/iterator.h"

#include "leveldb/slice.h"
#include "leveldb/status.h"
namespace leveldb {
Iterator::Iterator() {
  cleanup_head_.function = nullptr;
  cleanup_head_.next = nullptr;
}

Iterator::~Iterator() {
  // 头节点可能为空 但不一定
  // 头节点不用delete，该内存随析构自动释放
  if (!cleanup_head_.IsEmpty()) {
    cleanup_head_.Run();
    for (CleanupNode* node = cleanup_head_.next; node != nullptr;) {
      node->Run();
      CleanupNode* next_node = node->next;
      delete node;
      node = next_node;
    }
  }
}

void Iterator::RegisterCleanup(CleanupFunction func, void* arg1, void* arg2) {
  // using 可以从头文件传递，慎用
  assert(func != nullptr);
  CleanupNode* node;
  if (cleanup_head_.IsEmpty()) {
    node = &cleanup_head_;
  } else {
    node = new CleanupNode();
    node->next = cleanup_head_.next;
    cleanup_head_.next = node;
  }
  node->function = func;
  node->arg1 = arg1;
  node->arg2 = arg2;
}

namespace {
class EmptyIterator : public Iterator {
 public:
  explicit EmptyIterator(const Status& s) : status_(s) {}
  ~EmptyIterator() override = default;
  bool Valid() const override { return false; }
  void Seek(const Slice& target) override {}
  void SeekToFirst() override {}
  void SeekToLast() override {}
  void Next() override { assert(false); }
  void Prev() override { assert(false); }
  Slice key() const override {
    assert(false);
    return Slice();
  }
  Slice value() const override {
    assert(false);
    return Slice();
  }
  Status status() const override { return status_; }

 private:
  Status status_;
};
}  // namespace
Iterator* NewEmptyIterator() { return new EmptyIterator(Status::OK()); }
Iterator* NewErrorIterator(const Status& status) {
  return new EmptyIterator(status);
}

}  // namespace leveldb
