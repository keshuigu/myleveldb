#ifndef STORAGE_LEVELDB_DB_SKIPLIST_H_
#define STORAGE_LEVELDB_DB_SKIPLIST_H_

// 线程安全
// -------------
//
// 写操作需要外部同步，最可能的是使用互斥锁。
// 读操作需要保证在读取过程中 SkipList
// 不会被销毁。除此之外，读操作无需任何内部锁定或同步即可进行。
//
// 不变性：
//
// (1) 分配的节点在 SkipList
// 被销毁之前永远不会被删除。这在代码中是显而易见的，因为我们从不删除任何跳表节点。
//
// (2) 节点的内容在链接到 SkipList 之后是不可变的，除了 next/prev 指针。只有
// Insert()
// 修改列表，并且它会小心地初始化一个节点并使用释放存储来发布一个或多个列表中的节点。
//
// ... prev 与 next 指针顺序 ...

// 采用随机技术决定链表中哪些节点应增加向前指针以及在该节点中应增加多少个指针
#include <atomic>
#include <cassert>
#include <cstdlib>

#include "util/arena.h"
#include "util/random.h"

namespace leveldb {

template <typename Key, class Comparator>
class SkipList {
 private:
  struct Node;

 public:
  // 创建一个新的 SkipList 对象，该对象将使用 "cmp" 来比较键，
  // 并使用 "*arena" 分配内存。在 arena 中分配的对象必须在 SkipList
  // 对象的生命周期内保持分配状态。
  SkipList(Comparator cmp, Arena* arena);

  SkipList(const SkipList&) = delete;
  SkipList& operator=(const SkipList&) = delete;

  // 将键插入到列表中。
  // 要求：当前列表中没有与该键相等的元素。
  void Insert(const Key& key);

  bool Contains(const Key& key) const;

  class Iterator {
   public:
    explicit Iterator(const SkipList* list);

    bool Valid() const;
    const Key& key() const;

    void Next();

    void Prev();

    void Seek(const Key& target);

    void SeekToFirst();

    void SeekToLast();

   private:
    const SkipList* list_;
    Node* node_;
  };

 private:
  enum { kMaxHeight = 12 };

  inline int GetMaxHeight() const {
    return max_height_.load(std::memory_order_relaxed);
  }

  Node* NewNode(const Key& key, int height);
  int RandomHeight();
  bool Equal(const Key& a, const Key& b) const { return (compare_(a, b) == 0); }

  bool KeyIsAfterNode(const Key& key, Node* n) const;
  // 返回第一个大于或等于 key 的节点。
  // 如果没有这样的节点，则返回 nullptr。
  //
  // 如果prev非空，则在[0..max_height_-1]的每个级别中，用指向该级别上前一个节点的指针填充prev[level]。
  Node* FindGreaterOrEqual(const Key& key, Node** prev) const;

  // 找不到时返回head_
  Node* FindLessThan(const Key& key) const;

  Node* FindLast() const;

  Comparator const compare_;
  Arena* const arena_;
  Node* const head_;
  // 仅由 Insert() 修改。读操作可能会竞争，但过期的值是可以接受的。
  std::atomic<int> max_height_;  // 跳表高度

  Random rnd_;
};

template <typename Key, class Comparator>
struct SkipList<Key, Comparator>::Node {
  Key const key;

  explicit Node(const Key& k) : key(k) {}
  // 链表的访问器/修改器。包装在方法中，以便我们可以根据需要添加适当的屏障。
  Node* Next(int n) {
    assert(n >= 0);
    // std::memory_order_acquire用于加载操作，
    // 确保在加载操作之后的所有读写操作都不会被重排序到加载操作之前
    return next_[n].load(std::memory_order_acquire);
  }

  void SetNext(int n, Node* x) {
    assert(n >= 0);
    // std::memory_order_release用于存储操作，
    // 确保在存储操作之前的所有读写操作不会被重排序到存储操作之后。
    next_[n].store(x, std::memory_order_release);
  }

  Node* NoBarrierNext(int n) {
    assert(n >= 0);
    return next_[n].load(std::memory_order_relaxed);
  }
  void NoBarrierSetNext(int n, Node* x) {
    assert(n >= 0);
    next_[n].store(x, std::memory_order_relaxed);
  }

 private:
  // 数组的长度等于节点的高度。next_[0] 是最低级别的链接。
  std::atomic<Node*> next_[1];
};

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node* SkipList<Key, Comparator>::NewNode(
    const Key& key, int height) {
  // height - 1 是因为Node中已经占有一个std::atomic<Node*>
  // 此处为额外分配
  char* const node_memory = arena_->AllocateAligned(
      sizeof(Node) + sizeof(std::atomic<Node*>) * (height - 1));
  return new (node_memory) Node(key);
}

template <typename Key, class Comparator>
inline SkipList<Key, Comparator>::Iterator::Iterator(const SkipList* list) {
  list_ = list;
  node_ = nullptr;
}

template <typename Key, class Comparator>
inline bool SkipList<Key, Comparator>::Iterator::Valid() const {
  return node_ != nullptr;
}

template <typename Key, class Comparator>
inline const Key& SkipList<Key, Comparator>::Iterator::key() const {
  assert(Valid());
  return node_->key;
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Next() {
  assert(Valid());
  node_ = node_->Next(0);  // 高度0
}
template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Prev() {
  // 我们不使用显式的 "prev" 链接，而是只搜索在 key 之前的最后一个节点。
  assert(Valid());
  node_ = list_->FindLessThan(node_->key);
  if (node_ == list_->head_) {
    node_ = nullptr;
  }
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Seek(const Key& target) {
  node_ = list_->FindGreaterOrEqual(target, nullptr);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToFirst() {
  node_ = list_->head_->Next(0);  // 带有空头节点
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToLast() {
  node_ = list_->FindLast();
  if (node_ == list_->head_) {
    node_ = nullptr;
  }
}

template <typename Key, class Comparator>
int SkipList<Key, Comparator>::RandomHeight() {
  // 以 1/kBranching 的概率增加高度
  static const unsigned int kBranching = 4;
  int height = 1;
  while (height < kMaxHeight && rnd_.OneIn(kBranching)) {
    height++;
  }
  assert(height > 0);
  assert(height <= kMaxHeight);
  return height;
}

template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::KeyIsAfterNode(const Key& key, Node* n) const {
  // nullptr 认为是无穷大
  return (n != nullptr) && (compare_(n->key, key) < 0);
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
SkipList<Key, Comparator>::FindGreaterOrEqual(const Key& key,
                                              Node** prev) const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node* next = x->Next(level);
    // 向前迭代
    if (KeyIsAfterNode(key, next)) {
      x = next;
    } else {
      if (prev != nullptr) {
        prev[level] = x;
      }
      if (level == 0) {
        return next;
      } else {
        level--;
      }
    }
  }
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
SkipList<Key, Comparator>::FindLessThan(const Key& key) const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    assert(x == head_ || compare_(x->key, key) < 0);
    Node* next = x->Next(level);
    if (next == nullptr || compare_(next->key, key) >= 0) {
      if (level == 0) {
        return x;  // 已经是最后一个
      } else {
        level--;  // 这一层的下个节点超过key了
      }
    } else {
      x = next;
    }
  }
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node* SkipList<Key, Comparator>::FindLast()
    const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node* next = x->Next(level);
    if (next == nullptr) {
      if (level == 0) {
        return x;
      } else {
        // Switch to next list
        level--;
      }
    } else {
      x = next;
    }
  }
}

template <typename Key, class Comparator>
SkipList<Key, Comparator>::SkipList(Comparator cmp, Arena* arena)
    : compare_(cmp),
      arena_(arena),
      head_(NewNode(0, kMaxHeight)),
      max_height_(1),
      rnd_(0xdeadbeef) {
  for (int i = 0; i < kMaxHeight; i++) {
    head_->SetNext(i, nullptr);
  }
}

template <typename Key, class Comparator>
void SkipList<Key, Comparator>::Insert(const Key& key) {
  Node* prev[kMaxHeight];  // 每个level在x前面的node
  Node* x = FindGreaterOrEqual(key, prev);

  // 不允许重复键
  assert(x == nullptr || !Equal(key, x->key));

  int height = RandomHeight();
  if (height > GetMaxHeight()) {
    // 新的level，这一层还没有其他node
    for (int i = GetMaxHeight(); i < height; i++) {
      prev[i] = head_;
    }
    // 修改max_height_而不与并发读者进行任何同步是可以的。
    // 观察到max_height_新值的并发读者将看到head_中新级别指针的旧值（nullptr）或下面循环中设置的新值。
    // 在前一种情况下，读者将立即降到下一级别，因为 nullptr 排在所有键之后。
    // 在后一种情况下，读者将使用新节点。
    max_height_.store(height, std::memory_order_relaxed);
  }
  x = NewNode(key, height);
  for (int i = 0; i < height; i++) {
    // 插入节点
    // NoBarrier_SetNext()就足够了，因为当我们在prev[i]中发布指向 "x"
    // 的指针时会添加一个屏障。
    x->NoBarrierSetNext(i, prev[i]->NoBarrierNext(i));
    prev[i]->SetNext(i, x);
  }
}

template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::Contains(const Key& key) const {
  Node* x = FindGreaterOrEqual(key, nullptr);
  if (x != nullptr && Equal(key, x->key)) {
    return true;
  } else {
    return false;
  }
}
}  // namespace leveldb

#endif