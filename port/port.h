#ifndef STORAGE_LEVELDB_PORT_PORT_H_
#define STORAGE_LEVELDB_PORT_PORT_H_

#if defined(LEVELDB_PLATFORM_POSIX) || defined(LEVELDB_PLATFORM_WINDOWS) // #ifdef 不能进行逻辑组合判断
#include "port/port_stdcxx.h"
#endif

#endif // STORAGE_LEVELDB_PORT_PORT_H_