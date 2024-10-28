#ifndef STORAGE_LEVELDB_DB_FILENAME_H_
#define STORAGE_LEVELDB_DB_FILENAME_H_

#include <cstdint>
#include <string>

#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "port/port.h"

namespace leveldb {
class Env;

enum FileType {
  kLogFile,
  kDBLockFile,
  kTableFile,
  kDescriptorFile,
  kCurrentFile,
  kTempFile,
  kInfoLogFile
};

std::string LogFileName(const std::string& dbname, uint64_t number);

std::string TableFileName(const std::string& dbname, uint64_t number);

// sstable
std::string SSTTableFileName(const std::string& dbname, uint64_t number);

std::string DescriptorFileName(const std::string& dbname, uint64_t number);

// currentfile 包含 current manifest file的名字
std::string CurrentFileName(const std::string& dbname);

std::string LockFileName(const std::string& dbname);

std::string TempFileName(const std::string& dbname, uint64_t number);

std::string InfoLogFileName(const std::string& dbname);

std::string OldInfoLogFileName(const std::string& dbname);

// 判断文件是否为leveldb文件，将文件的类型存放于*type中，文件名中的数字保存在*number中
bool ParseFileName(const std::string& filename, uint64_t* number,
                   FileType* type);

Status SetCurrentFile(Env* env, const std::string& dbname,
                      uint64_t descriptor_number);
}  // namespace leveldb

#endif