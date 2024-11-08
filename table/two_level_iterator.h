#ifndef STORAGE_LEVELDB_TABLE_TWO_LEVEL_ITERATOR_H_
#define STORAGE_LEVELDB_TABLE_TWO_LEVEL_ITERATOR_H_

#include "leveldb/iterator.h"

namespace leveldb {
struct ReadOptions;
// 返回一个新的两级迭代器。两级迭代器包含一个索引迭代器，
// 其值指向一系列块，每个块本身是一系列键/值对。
// 返回的两级迭代器生成块序列中所有键/值对的连接。
// 接受 "index_iter" 的所有权，并在不再需要时删除它。
//
// 使用提供的block_function函数将 index_iter 的值转换为对应块内容的迭代器。
Iterator* NewTwoLevelIterator(
    Iterator* index_iter,
    Iterator* (*block_function)(void* arg, const ReadOptions& options,
                                const Slice& index_value),
    void* arg, const ReadOptions& options);
}  // namespace leveldb

#endif