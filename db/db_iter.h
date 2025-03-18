#ifndef STORAGE_LEVELDB_DB_DB_ITER_H_
#define STORAGE_LEVELDB_DB_DB_ITER_H_

#include <cstdint>

#include "db/dbformat.h"
#include "leveldb/db.h"

namespace leveldb {
class DBImpl;

// 返回一个新的迭代器，该迭代器将指定 "sequence" 号时存活的内部键（由
// "*internal_iter" 生成）转换为适当的用户键。
Iterator* NewIterator(DBImpl* db, const Comparator* user_key_comparator,
                      Iterator* internal_iter, SequenceNumber sequence,
                      uint32_t seed);
}  // namespace leveldb

#endif