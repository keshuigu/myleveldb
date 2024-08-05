cmake_minimum_required(VERSION 3.9)

project(myleveldb VERSION 0.0.1 LANGUAGES C CXX)

if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED OFF)
  set(CMAKE_C_EXTENSIONS OFF)
endif()

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED OFF)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

if (WIN32)
  # TODO
  message( FATAL_ERROR "Not Implement")
else()
  set(LEVELDB_PLATFORM_NAME LEVELDB_PLATFORM_POSIX)
endif()

option(LEVELDB_BUILD_TESTS "Build LevelDB's unit tests" ON)
option(LEVELDB_BUILD_BENCHMARKS "Build LevelDB's benchmarks" ON)
option(LEVELDB_INSTALL "Install LevelDB's header and library" OFF)


# 检查头文件是否存在
include(CheckIncludeFile)
check_include_file("unistd.h" HAVE_UNISTD_H)
# 检查以下库是否存在
include(CheckLibraryExists)
check_library_exists(crc32c crc32c_value "/lib/x86_64-linux-gnu/" HAVE_CRC32C) # 找不到
check_library_exists(snappy snappy_compress "/lib/x86_64-linux-gnu/" HAVE_SNAPPY)
check_library_exists(zstd zstd_compress "/lib/x86_64-linux-gnu/" HAVE_ZSTD)
check_library_exists(tcmalloc malloc "/lib/x86_64-linux-gnu/" HAVE_TCMALLOC) # 找不到