#ifndef STORAGE_LEVELDB_DB_MEMTABLE_H_
#define STORAGE_LEVELDB_DB_MEMTABLE_H_

#include <string>

#include "db/dbformat.h"
#include "db/skiplist.h"
#include "leveldb/db.h"
#include "util/arena.h"

namespace leveldb {

class InternalKeyComparator;
class Iterator;

class MemTable {
  // MemTables are reference counted.  The initial reference count
  // is zero and the caller must call Ref() at least once.
  explicit MemTable(const InternalKeyComparator& comparator);

  MemTable(const MemTable&) = delete;
  MemTable& operator=(const MemTable&) = delete;

  // Increase reference count.
  void Ref() { ++refs_; }

  void Unref() {
    --refs_;
    assert(refs_ >= 0);
    if (refs_ <= 0) {
      delete this;
    }
  }
  // 返回此数据结构使用的数据字节数的估计值。
  // 在 MemTable 被修改时调用是安全的。
  size_t ApproximateMemoryUsage();

  // 返回一个迭代器，该迭代器生成 memtable 的内容。
  //
  // 调用者必须确保在返回的迭代器存活期间，底层的 MemTable 也保持存活。
  // 该迭代器返回的键是由db/format.{h,cc}模块中的AppendInternalKey编码的内部键。
  Iterator* NewIterator();

  // 向 memtable 中添加一个条目，该条目将键映射到指定序列号和指定类型的值。
  // 通常，如果 type==kTypeDeletion，value 将为空。
  void Add(SequenceNumber seq, ValueType type, const Slice& key,
           const Slice& value);

  // 如果memtable包含键的值，则将其存储在*value中并返回 true。
  // 如果memtable包含键的删除记录，则在*status中存储一个NotFound()错误并返回true。
  // 否则，返回 false。
  bool Get(const LookupKey& key, std::string* value, Status* s);

 private:
  friend class MemTableIterator;
  struct KeyComparator {
    const InternalKeyComparator comparator;
    explicit KeyComparator(const InternalKeyComparator& c) : comparator(c) {}
    int operator()(const char* a, const char* b) const;
  };

  typedef SkipList<const char*, KeyComparator> Table;
  ~MemTable();
  KeyComparator comparator_;
  int refs_;
  Arena arena_;
  Table table_;
};

}  // namespace leveldb

#endif