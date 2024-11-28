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

set(LEVELDB_PUBLIC_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include/leveldb") # 公共include 目录
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
    "db/dbformat.h"
    "db/filename.cc"
    "db/filename.h"
    "db/version_edit.h"
    "db/version_edit.cc"
    "port/port_stdcxx.h"
    "port/port.h"
    "port/thread_annotations.h"
    "table/block_builder.cc"
    "table/block_builder.h"
    "table/block.cc"
    "table/block.h"
    "table/iterator.cc"
    "table/filter_block.cc"
    "table/filter_block.h"
    "table/format.cc"
    "table/format.h"
    "table/iterator_wrapper.h"
    "table/two_level_iterator.h"
    "table/two_level_iterator.cc"
    "util/arena.cc"
    "util/arena.h"
    "util/bloom.cc"
    "util/status.cc"
    "util/env.cc"
    "util/random.h"
    "util/mutexlock.h"
    "util/logging.h"
    "util/logging.cc"
    "util/coding.h"
    "util/coding.cc"
    "util/hash.cc"
    "util/hash.h"
    "util/cache.cc"
    "util/comparator.cc"
    "util/no_destructor.h"
    "util/filter_policy.cc"
    "util/crc32c.cc"
    "util/crc32c.h"
    "util/histogram.cc"
    "util/histogram.h"
  PUBLIC
    # TODO
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/env.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/export.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/slice.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/status.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/dumpfile.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/db.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/options.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/cache.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/comparator.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/filter_policy.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/table_builder.h"
    "${LEVELDB_PUBLIC_INCLUDE_DIR}/iterator.h"
)

if(WIN32)
  # TODO
  message( FATAL_ERROR "Not Implement")
else()
  target_sources(leveldb
    PRIVATE

      "util/env_posix.cc"
      "util/posix_logger.h"
  )
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
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
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
# Install


# Google Test
if(LEVELDB_BUILD_TESTS)
  enable_testing()
  # Prevent overriding the parent project's compiler/linker settings on Windows.
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(install_gtest OFF)
  set(install_gmock OFF)
  set(build_gmock ON)

  add_subdirectory("third_party/googletest")
  # GoogleTest triggers a missing field initializers warning.
  if(LEVELDB_HAVE_NO_MISSING_FIELD_INITIALIZERS)
    set_property(TARGET gtest
        APPEND PROPERTY COMPILE_OPTIONS -Wno-missing-field-initializers)
    set_property(TARGET gmock
        APPEND PROPERTY COMPILE_OPTIONS -Wno-missing-field-initializers)
  endif(LEVELDB_HAVE_NO_MISSING_FIELD_INITIALIZERS)

  add_executable(leveldb_tests "")
  target_sources(leveldb_tests
    PRIVATE
      "${PROJECT_BINARY_DIR}/${LEVELDB_PORT_CONFIG_DIR}/port_config.h"
      "util/env_test.cc"
      "util/status_test.cc"
      "util/testutil.h"
      "util/testutil.cc"
      "util/no_destructor_test.cc"
  )
  if(NOT BUILD_SHARED_LIBS)
    target_sources(leveldb_tests
      PRIVATE
        "db/filename_test.cc"
        "util/bloom_test.cc"
        # TODO
        "util/logging_test.cc"
        "util/coding_test.cc"
        "util/hash_test.cc"
        "util/cache_test.cc"
        "util/crc32c_test.cc"
        "util/arena_test.cc"
        "table/filter_block_test.cc"
    )
  endif()
  target_link_libraries(leveldb_tests leveldb gmock gtest gtest_main)
  target_compile_definitions(leveldb_tests
    PRIVATE
    ${LEVELDB_PLATFORM_NAME}=1
  )
  if (NOT HAVE_CXX17_HAS_INCLUDE)
    target_compile_definitions(leveldb_tests
      PRIVATE
        LEVELDB_HAS_PORT_CONFIG_H=1
    )
  endif()

  add_test(NAME "level_tests" COMMAND "leveldb_tests")

  function(leveldb_test test_file)
    get_filename_component(test_target_name "${test_file}" NAME_WE)
    add_executable("${test_target_name}" "")
    target_sources("${test_target_name}"
      PRIVATE
        "${PROJECT_BINARY_DIR}/${LEVELDB_PORT_CONFIG_DIR}/port_config.h"
        "util/testutil.cc"
        "util/testutil.h"

        "${test_file}"
    )
    target_link_libraries("${test_target_name}" leveldb gmock gtest)
    target_compile_definitions("${test_target_name}"
      PRIVATE
        ${LEVELDB_PLATFORM_NAME}=1
    )
    if (NOT HAVE_CXX17_HAS_INCLUDE)
      target_compile_definitions("${test_target_name}"
        PRIVATE
          LEVELDB_HAS_PORT_CONFIG_H=1
      )
    endif()
    add_test(NAME "${test_target_name}" COMMAND "${test_target_name}")
  endfunction()
  # TODO
  # leveldb_test("db/c_test.cc")
  if(NOT BUILD_SHARED_LIBS)
    # TODO(costan): This test also uses
    #               "util/env_{posix|windows}_test_helper.h"
    # if (WIN32)
    #   leveldb_test("util/env_windows_test.cc")
    # else (WIN32)
      leveldb_test("util/env_posix_test.cc")
      target_sources(env_posix_test
        PRIVATE
        "util/env_posix_test_helper.h"
      )
    # endif (WIN32)
  endif(NOT BUILD_SHARED_LIBS)
endif()

# Google Benchmark
if(LEVELDB_BUILD_BENCHMARKS)
  # This project uses Google benchmark for benchmarking.
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)
  add_subdirectory("third_party/benchmark")

  function(leveldb_benchmark bench_file)
    get_filename_component(bench_target_name "${bench_file}" NAME_WE)

    add_executable("${bench_target_name}" "")
    target_sources("${bench_target_name}"
      PRIVATE
        "${PROJECT_BINARY_DIR}/${LEVELDB_PORT_CONFIG_DIR}/port_config.h"
        "util/histogram.cc"
        "util/histogram.h"
        "util/testutil.cc"
        "util/testutil.h"

        "${bench_file}"
    )
    target_link_libraries("${bench_target_name}" leveldb gmock gtest benchmark)
    target_compile_definitions("${bench_target_name}"
      PRIVATE
        ${LEVELDB_PLATFORM_NAME}=1
    )
    if (NOT HAVE_CXX17_HAS_INCLUDE)
      target_compile_definitions("${bench_target_name}"
        PRIVATE
          LEVELDB_HAS_PORT_CONFIG_H=1
      )
    endif()
  endfunction()

  if(NOT BUILD_SHARED_LIBS)
    # leveldb_benchmark("benchmarks/db_bench.cc")
  endif()
# 对比测试
#   check_library_exists(sqlite3 sqlite3_open "" HAVE_SQLITE3)
#   if(HAVE_SQLITE3)
#     leveldb_benchmark("benchmarks/db_bench_sqlite3.cc")
#     target_link_libraries(db_bench_sqlite3 sqlite3)
#   endif()
#   # check_library_exists is insufficient here because the library names have
#   # different manglings when compiled with clang or gcc, at least when installed
#   # with Homebrew on Mac.
#   set(OLD_CMAKE_REQURED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
#   list(APPEND CMAKE_REQUIRED_LIBRARIES kyotocabinet)
#   check_cxx_source_compiles("
# #include <kcpolydb.h>

# int main() {
#   kyotocabinet::TreeDB* db = new kyotocabinet::TreeDB();
#   delete db;
#   return 0;
# }
#   "  HAVE_KYOTOCABINET)
#   set(CMAKE_REQUIRED_LIBRARIES ${OLD_CMAKE_REQURED_LIBRARIES})
#   if(HAVE_KYOTOCABINET)
#     leveldb_benchmark("benchmarks/db_bench_tree_db.cc")
#     target_link_libraries(db_bench_tree_db kyotocabinet)
#   endif()
endif()

# Install

if(LEVELDB_INSTALL)
  install(TARGETS leveldb
    EXPORT leveldbTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
  install(
    FILES
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/c.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/cache.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/comparator.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/db.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/dumpfile.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/env.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/export.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/filter_policy.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/iterator.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/options.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/slice.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/status.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/table_builder.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/table.h"
      "${LEVELDB_PUBLIC_INCLUDE_DIR}/write_batch.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/leveldb"
  )

  include(CMakePackageConfigHelpers)
  configure_package_config_file(
    "cmake/${PROJECT_NAME}Config.cmake.in"
    "${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  )
  write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
  )
  install(
    EXPORT leveldbTargets
    NAMESPACE leveldb::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  )
  install(
    FILES
      "${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
      "${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  )
endif(LEVELDB_INSTALL)
