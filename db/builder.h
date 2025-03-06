#ifndef STORAGE_LEVELDB_DB_BUILDER_H_
#define STORAGE_LEVELDB_DB_BUILDER_H_

#include "leveldb/status.h"

namespace leveldb {
struct Options;
struct FileMetaData;
class Env;
class Iterator;
class TableCache;
class VersionEdit;

// 从 *iter 的内容构建一个表文件。生成的文件将根据 meta->number 命名。
// 成功后，*meta 的其余部分将填充生成表的元数据。
// 如果 *iter 中没有数据，meta->file_size 将被设置为零，并且不会生成表文件。
Status BuildTable(const std::string& dbname, Env* env, const Options& options,
                  TableCache* table_cache, Iterator* iter, FileMetaData* meta);
}  // namespace leveldb

#endif