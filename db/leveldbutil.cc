#include <iostream>
#include "leveldb/slice.h"
#include "leveldb/status.h"
int main() {
  leveldb::Status s = leveldb::Status::IOError(leveldb::Slice("hello"),leveldb::Slice("Status"));
  std::cout << s.ToString() << std::endl;
  return 0;
}