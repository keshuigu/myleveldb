#ifndef STORAGE_LEVELDB_DB_TABLE_CACHE_H_
#define STORAGE_LEVELDB_DB_TABLE_CACHE_H_

#include <cstdint>
#include <string>

#include "db/dbformat.h"
#include "leveldb/cache.h"
#include "leveldb/table.h"
#include "port/port.h"

namespace leveldb {

class Env;

class TableCache {
 public:
  TableCache(const std::string& dbname, const Options& options, int entries);

  TableCache(const TableCache&) = delete;

  TableCache& operator=(const TableCache&) = delete;

  ~TableCache();

  // 为指定的文件编号返回一个迭代器（对应的文件长度必须正好是"file_size"字节
  // 如果"tableptr"非空，还会将"*tableptr"设置为指向返回的迭代器所基于的Table对象
  // 如果没有Table对象，则设置为nullptr
  // 返回的"*tableptr"对象由缓存拥有，不应被删除，并且在返回的迭代器存活期间有效。
  Iterator* NewIterator(const ReadOptions& options, uint64_t file_number,
                        uint64_t file_size, Table** tableptr = nullptr);

  // 如果在指定文件中查找内部键 "k" 找到了一个条目，
  // 则调用 (*handle_result)(arg, found_key, found_value)。
  Status Get(const ReadOptions& options, uint64_t file_number,
             uint64_t file_size, const Slice& k, void* arg,
             void (*handle_result)(void*, const Slice&, const Slice&));

  // 驱逐指定文件编号的任何条目
  void Evict(uint64_t file_number);

 private:
  Status FindTable(uint64_t file_number, uint64_t file_size, Cache::Handle**);

  Env* const env_;
  const std::string dbname_;
  const Options& options_;
  Cache* cache_;
};

}  // namespace leveldb

#endif