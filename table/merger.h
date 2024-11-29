#ifndef STORAGE_LEVELDB_TABLE_MERGER_H_
#define STORAGE_LEVELDB_TABLE_MERGER_H_

namespace leveldb {
class Comparator;
class Iterator;

// 返回一个迭代器，该迭代器提供 children[0,n-1] 中数据的并集。接管子迭代器的所有权，并在结果迭代器被删除时删除它们。
//
// 结果不做去重。即，如果某个键在 K 个子迭代器中存在，它将被产出 K 次。
Iterator* NewMergingIterator(const Comparator* comparator, Iterator** children,
                             int n);

}  // namespace leveldb

#endif