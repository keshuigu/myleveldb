// LRU Cache
#ifndef STORAGE_LEVELDB_INCLUDE_CACHE_H_
#define STORAGE_LEVELDB_INCLUDE_CACHE_H_

#include <cstdint>

#include "leveldb/export.h"
#include "leveldb/slice.h"

namespace leveldb {
class LEVELDB_EXPORT Cache;

LEVELDB_EXPORT Cache* NewLRUCache(size_t capacity);
class LEVELDB_EXPORT Cache {
 public:
  Cache() = default;
  Cache(const Cache&) = delete;
  Cache& operator=(const Cache&) = delete;

  virtual ~Cache();

  struct Handle {};

  // 以指定的容量消耗插入 key -> value 的映射条目
  // 返回对应的Handle
  virtual Handle* Insert(const Slice& key, void* value, size_t charge,
                         void (*deleter)(const Slice& key, void* value)) = 0;

  // 返回对应于key相关联映射的Handle
  virtual Handle* Lookup(const Slice& key) = 0;

  // 释放一组映射
  // REQUIRES: Handle 未被释放
  // REQUIRES: Handle 必须为*this的方法返回值
  virtual void Release(Handle* handle) = 0;

  // 返回映射的value
  // REQUIRES: Handle 未被释放
  // REQUIRES: Handle 必须为*this的方法返回值
  virtual void* Value(Handle* handle) = 0;

  // 擦除key的映射，条目会在所有相关的Handle释放后释放
  virtual void Erase(const Slice& key) = 0;

  // 返回一个新的id。用于多个客户端使用共享同一缓存以划分密钥空间。通常
  // 客户端将在启动时分配一个新的id，并将id添加到其缓存密钥。
  virtual uint64_t NewId() = 0;

  // 删除所有未使用的条目
  // 默认情况下不操作
  virtual void Prune() {}

  // 估计cache的消耗
  virtual size_t TotalCharge() const = 0;
};

}  // namespace leveldb

#endif