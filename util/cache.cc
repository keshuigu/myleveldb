#include "leveldb/cache.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/hash.h"
#include "util/mutexlock.h"

namespace leveldb {
Cache::~Cache() {}
namespace {
// LRU cache implementation
//
// Cache entries have an "in_cache" boolean indicating whether the cache has a
// reference on the entry.  The only ways that this can become false without the
// entry being passed to its "deleter" are via Erase(), via Insert() when
// an element with a duplicate key is inserted, or on destruction of the cache.
//
// The cache keeps two linked lists of items in the cache.  All items in the
// cache are in one list or the other, and never both.  Items still referenced
// by clients but erased from the cache are in neither list.  The lists are:
// - in-use:  contains the items currently referenced by clients, in no
//   particular order.  (This list is used for invariant checking.  If we
//   removed the check, elements that would otherwise be on this list could be
//   left as disconnected singleton lists.)
// - LRU:  contains the items not currently referenced by clients, in LRU order
// Elements are moved between these lists by the Ref() and Unref() methods,
// when they detect an element in the cache acquiring or losing its only
// external reference.

// 条目是一种可变长度的堆分配结构。条目保存在按访问时间排序的循环双链表中。
struct LRUHandle {
  void* value;
  void (*deleter)(const Slice& key, void* value);
  LRUHandle* next_hash;
  LRUHandle* next;
  LRUHandle* prev;
  size_t charge;
  size_t key_length;
  bool in_cache;     // 是否在缓存中
  uint32_t refs;     // 引用计数
  uint32_t hash;     // key的hash值，用于快速分片与比较
  char key_data[1];  // key的开始位置
  Slice key() const {
    // 只有空链表的头节点的next可能与this相同
    assert(next != this);
    return Slice(key_data, key_length);
  }
};
// We provide our own simple hash table since it removes a whole bunch
// of porting hacks and is also faster than some of the built-in hash
// table implementations in some of the compiler/runtime combinations
// we have tested.  E.g., readrandom speeds up by ~5% over the g++
// 4.4.3's builtin hashtable.
class HandleTable {
 public:
  HandleTable() {}

 private:
  // HandleTable维护多个buckets，每个bucket由hash索引，维护一个cache条目的链表
  uint32_t length_;
  uint32_t elems_;
  LRUHandle** list_;

  // 通过key或者hash值来寻找cache条目
  // 如果不存在，返回指向对应链表的尾端指针
  LRUHandle** FindPointer(const Slice& key, uint32_t hash) {
    LRUHandle** ptr = &list_[hash & (length_ - 1)];  // length_ 永远为2的倍数
    while (*ptr != nullptr && ((*ptr)->hash != hash || key != (*ptr)->key())) {
      ptr = &(*ptr)->next_hash;
    }
    return ptr;
  }

  void Resize() {
    uint32_t new_length = 4;
    while (new_length < elems_) {
      new_length *= 2;
    }
    LRUHandle** new_list = new LRUHandle*[new_length];
    memset(new_list, 0, sizeof(new_list[0]) * new_length);
    uint32_t count = 0;
    for (uint32_t i = 0; i < length_; i++) {
      LRUHandle* h = list_[i];
      while (h != nullptr) {
        LRUHandle* next = h->next_hash;  // 记录原来的下个指针
        uint32_t hash = h->hash;
        // 重新以新的长度进行索引，ptr是指向list中的位置
        LRUHandle** ptr = &new_list[hash & (new_length - 1)];
        // 头插法
        h->next_hash = *ptr;  // 将原本该处的指针连接到h之后
        *ptr = h;             // 该位置改为h
        h = next;             // 迭代
        count++;
      }
    }
    assert(elems_ == count);
    delete[] list_;
    list_ = new_list;
    length_ = new_length;
  }
};

}  // namespace
}  // namespace leveldb
