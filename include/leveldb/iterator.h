#ifndef STORAGE_LEVELDB_INCLUDE_ITERATOR_H_
#define STORAGE_LEVELDB_INCLUDE_ITERATOR_H_

// 迭代器从源中生成一系列键/值对。
// 以下类定义了接口。本库提供了多种实现。
// 特别是，提供了访问 Table 或 DB 内容的迭代器。
//
// 多个线程可以在不进行外部同步的情况下调用迭代器的 const 方法，
// 但如果任何线程可能调用非 const
// 方法，则所有访问同一迭代器的线程必须使用外部同步。
#include <cassert>

#include "leveldb/export.h"
namespace leveldb {
class Slice;
class Status;
class LEVELDB_EXPORT Iterator {
 public:
  Iterator();
  Iterator(const Iterator&) = delete;
  Iterator& operator=(const Iterator&) = delete;

  virtual ~Iterator();

  // 迭代器要么定位在一个键/值对上，要么无效。
  // 仅当迭代器有效时，此方法才返回 true。
  virtual bool Valid() const = 0;

  // 定位至第一个key
  virtual void SeekToFirst() = 0;

  // 定位至最后一个key
  virtual void SeekToLast() = 0;

  // 定位至源中第一个大于或等于 target 的键。
  // 如果源中包含一个大于或等于 target 的条目，则此调用后迭代器有效。
  virtual void Seek(const Slice& target) = 0;

  // 调用后，如果迭代器位于最后一个词条，则Valid为false
  virtual void Next() = 0;

  // 调用后，如果迭代器位于第一个词条，则Valid为false
  virtual void Prev() = 0;

  // 返回当前条目的键。返回的 slice 的底层存储仅在迭代器的下一次修改之前有效。
  virtual Slice key() const = 0;

  // 返回当前条目的值。返回的 slice 的底层存储仅在迭代器的下一次修改之前有效。
  virtual Slice value() const = 0;

  virtual Status status() const = 0;

  // 客户端可以注册在销毁此迭代器时调用的 function/arg1/arg2 三元组。
  //
  // 请注意，与前面的所有方法不同，此方法不是抽象的，因此客户端不应重写它。
  using CleanupFunction = void (*)(void* arg1, void* arg2);
  void RegisterCleanup(CleanupFunction function, void* arg1, void* arg2);

 private:
  // 清理函数存储在单链表中。
  // 链表的头节点内联在迭代器中。
  struct CleanupNode {
    bool IsEmpty() const { return function == nullptr; }
    void Run() {
      assert(function != nullptr);
      (*function)(arg1, arg2);
    }

    CleanupFunction function;
    void* arg1;
    void* arg2;
    CleanupNode* next;
  };
  CleanupNode cleanup_head_;
};

// 返回一个空迭代器（不产生任何内容）。
LEVELDB_EXPORT Iterator* NewEmptyIterator();

// 返回具有指定状态的空迭代器。
LEVELDB_EXPORT Iterator* NewErrorIterator(const Status& status);

}  // namespace leveldb

#endif