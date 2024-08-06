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
check_library_exists(crc32c crc32c_value "" HAVE_CRC32C) # 找不到
check_library_exists(snappy snappy_compress "" HAVE_SNAPPY) # 其实找得到，但是这里检查不到，可以设置HAVE_SNAPPY为1来直接编译
check_library_exists(zstd zstd_compress "" HAVE_ZSTD) # 找不到
check_library_exists(tcmalloc malloc "" HAVE_TCMALLOC) # 找不到
# 检查以下函数是否存在
include(CheckCXXSymbolExists)
check_cxx_symbol_exists(fdatasync "unistd.h" HAVE_FDATASYNC)
check_cxx_symbol_exists(F_FULLFSYNC "fcntl.h" HAVE_FULLFSYNC)
check_cxx_symbol_exists(O_CLOEXEC "fcntl.h" HAVE_O_CLOEXEC)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # TODO
  message( FATAL_ERROR "Not Implement")
else()
  # Enable strict prototype warnings for C code in clang and gcc.
  if(NOT CMAKE_C_FLAGS MATCHES "-Wstrict-prototypes")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstrict-prototypes")
  endif()

  # Disable C++ exceptions.
  string(REGEX REPLACE "-fexceptions" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

  # Disable RTTI.
  string(REGEX REPLACE "-frtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()


# Test whether -Wthread-safety is available.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wthread-safety HAVE_CLANG_THREAD_SAFETY)

# Used by googletest.
check_cxx_compiler_flag(-Wno-missing-field-initializers
                        LEVELDB_HAVE_NO_MISSING_FIELD_INITIALIZERS)

include(CheckCXXSourceCompiles)
# Test whether C++17 __has_include is available.
check_cxx_source_compiles("
#if defined(__has_include) &&  __has_include(<string>)
#include <string>
#endif
int main() { std::string str; return 0; }
" HAVE_CXX17_HAS_INCLUDE)

set(LEVELDB_PUBLIC_INCLUDE_DIR "include/leveldb") # 公共include 目录
set(LEVELDB_PORT_CONFIG_DIR "include/port")

# 生成头文件
configure_file(
  "port/port_config.h.in"
  "${PROJECT_BINARY_DIR}/${LEVELDB_PORT_CONFIG_DIR}/port_config.h"
)

# 增加编译时的include目录
include_directories(
  "${PROJECT_BINARY_DIR}/include"
  "."
)

# 生成动态库时仅导出带有LEVELDB_EXPORT的符号
if(BUILD_SHARED_LIBS)
  # Only export LEVELDB_EXPORT symbols from the shared library.
  add_compile_options(-fvisibility=hidden)
endif()

# Must be included before CMAKE_INSTALL_INCLUDEDIR is used.
include(GNUInstallDirs)

# 组合使用
add_library(leveldb "")
target_sources(leveldb
  PRIVATE
    "${PROJECT_BINARY_DIR}/${LEVELDB_PORT_CONFIG_DIR}/port_config.h"
    # TODO
    "db/builder.cc"
  PUBLIC
    # TODO
)

if(WIN32)
  # TODO
  message( FATAL_ERROR "Not Implement")
else()
  # TODO
  # target_sources(leveldb
  #   PRIVATE
  #     "util/env_posix.cc"
  #     "util/posix_logger.h"
  # )
endif()

# TODO
# MemEnv is not part of the interface and could be pulled to a separate library.
# target_sources(leveldb
#   PRIVATE
#     "helpers/memenv/memenv.cc"
#     "helpers/memenv/memenv.h"
# )

target_include_directories(leveldb
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(leveldb
  PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_compile_definitions(leveldb
  PRIVATE
  # Used by include/export.h when building shared libraries.
  LEVELDB_COMPILE_LIBRARY
  # Used by port/port.h.
  ${LEVELDB_PLATFORM_NAME}=1
)

if (NOT HAVE_CXX17_HAS_INCLUDE)
  target_compile_definitions(leveldb
    PRIVATE
      LEVELDB_HAS_PORT_CONFIG_H=1
  )
endif()

if(BUILD_SHARED_LIBS)
  target_compile_definitions(leveldb
    PUBLIC
      # Used by include/export.h.
      LEVELDB_SHARED_LIBRARY
  )
endif()

if(HAVE_CLANG_THREAD_SAFETY)
  target_compile_options(leveldb
    PUBLIC
      -Werror -Wthread-safety)
endif()


if(HAVE_CRC32C)
  target_link_libraries(leveldb crc32c)
endif(HAVE_CRC32C)
if(HAVE_SNAPPY)
  target_link_libraries(leveldb snappy)
endif(HAVE_SNAPPY)
if(HAVE_ZSTD)
  target_link_libraries(leveldb zstd)
endif(HAVE_ZSTD)
if(HAVE_TCMALLOC)
  target_link_libraries(leveldb tcmalloc)
endif(HAVE_TCMALLOC)

# Needed by port_stdcxx.h
find_package(Threads REQUIRED)
target_link_libraries(leveldb Threads::Threads)


add_executable(leveldbutil
  "db/leveldbutil.cc"
)

target_link_libraries(leveldbutil leveldb)

# TODO
# Google Test
# Google Benchmark
# Install