#ifndef STORAGE_LEVELDB_INCLUDE_COMPARATOR_H_
#define STORAGE_LEVELDB_INCLUDE_COMPARATOR_H_

#include <string>

#include "leveldb/export.h"

namespace leveldb {
class Slice;

// 比较器提供确定key的先后顺序的方法
// 实现类必须是线程安全的
class LEVELDB_EXPORT Comparator {
 public:
  virtual ~Comparator();
  // 三路比较
  //   < 0 iff "a" < "b",
  //   == 0 iff "a" == "b",
  //   > 0 iff "a" > "b"
  virtual int Compare(const Slice& a, const Slice& b) const = 0;

  // 比较器的名字，用于检查数据库是否与比较器配对
  // 当实现类的变化会导致key顺序变化时，name应该随之变化
  // "leveldb" 是保留的关键词，不应被使用
  virtual const char* Name() const = 0;

  // 进阶函数：用于减少内部数据结构的占用空间，如index
  // blocks.各函数参数大小比较基于比较器获得

  // 如果 *start < limit, 修改 *start 为一个在[start,limit)中更短的string
  virtual void FindShortestSeparator(std::string* start,
                                     const Slice& limit) const = 0;

  // 将*key 修改为一个更短的字符串且 >= *key
  virtual void FindShortSuccessor(std::string* key) const = 0;
};

// 返回一个使用字典序的内置比较器。返回值不可删除。
LEVELDB_EXPORT const Comparator* BytewiseComparator();

}  // namespace leveldb

#endif