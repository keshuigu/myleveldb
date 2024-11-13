#include <algorithm>
#include <cstdio>
#include <iostream>

#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "util/posix_logger.h"
int main() {
  double a[5] = {1, 2, 3, 4, 5};
  double* x = std::upper_bound(std::begin(a), std::end(a), 3);
  std::cout << x - std::begin(a) << std::endl;
  // std::FILE* fp_ = fopen("/mnt/share/workspace/myleveldb/testlog", "w");
  // leveldb::Logger* info_log = new leveldb::PosixLogger(fp_);
  // leveldb::Log(info_log,"hello_log %s","leveldb log test");
  // leveldb::Status s = leveldb::Status::IOError(leveldb::Slice("hello"),
  //                                              leveldb::Slice("Status"));
  // std::cout << s.ToString() << std::endl;
  // fclose(fp_);
  return 0;
}