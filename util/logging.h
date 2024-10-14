#ifndef STORAGE_LEVELDB_UTIL_LOGGING_H_
#define STORAGE_LEVELDB_UTIL_LOGGING_H_

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "port/port.h"
namespace leveldb {

class Slice;

void AppendNumberTo(std::string* str, uint64_t num);

// Escapes any non-printable characters found in "value".
void AppendEscapedStringTo(std::string* str, const Slice& value);

std::string NumberToString(uint64_t num);

// Escapes any non-printable characters found in "value".
std::string EscapeString(const Slice& value);

// 将字符串转换为数字，可能会对*in造成破坏
// 不处理符号'-'
// 从字符串开头处理，转换出数字后缩短字符串
// "123AAA" -> *in = "AAA"; *val = 123;
bool ConsumeDecimalNumber(Slice* in, uint64_t* val);

}  // namespace leveldb

#endif