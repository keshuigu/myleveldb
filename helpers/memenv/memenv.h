#ifndef STORAGE_LEVELDB_HELPERS_MEMENV_MEMENV_H_
#define STORAGE_LEVELDB_HELPERS_MEMENV_MEMENV_H_

#include "leveldb/env.h"
#include "leveldb/export.h"
namespace leveldb {

// 返回一个将数据保存在内存中的Env，不变动非文件任务
// 调用者需要在使用后释放该Env
// 在返回值生命周期内 base_env 需要保持存活
LEVELDB_EXPORT Env* NewMemEnv(Env* base_env);
}  // namespace leveldb

#endif