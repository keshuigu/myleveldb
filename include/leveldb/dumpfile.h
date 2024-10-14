#ifndef STORAGE_LEVELDB_INCLUDE_DUMPFILE_H_
#define STORAGE_LEVELDB_INCLUDE_DUMPFILE_H_
#include <string>

#include "leveldb/env.h"
#include "leveldb/export.h"
#include "leveldb/status.h"

namespace leveldb {

// 以文本形式转储fname文件内容到*dst，通过调用dst->Append()方法实现。
// 每次调用都会传递一个以换行符结尾的文本，这些文本对应于文件中找到的单个项目。
// 此外，如果 fname 指定的文件不是一个 LevelDB
// 存储文件，或者文件无法读取时，就会返回非 OK 结果。
LEVELDB_EXPORT Status DumpFile(Env* env, const std::string& fname,
                               WritableFile* dst);

}  // namespace leveldb

#endif