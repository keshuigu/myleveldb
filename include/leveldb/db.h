#ifndef STORAGE_LEVELDB_INCLUDE_DB_H_
#define STORAGE_LEVELDB_INCLUDE_DB_H_

#include <cstdint>
#include <cstdio>

#include "leveldb/export.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"

namespace leveldb {
// 版本号与CMakeLists.txt保持一致
static const int kMajorVersion = 0;
static const int kMinorVersion = 1;

class LEVELDB_EXPORT Snapshot {
 protected:
  virtual ~Snapshot();
};

}  // namespace leveldb

#endif