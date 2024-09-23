#include "leveldb/env.h"

#include "gtest/gtest.h"
#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/mutexlock.h"
#include "util/testutil.h"

namespace leveldb {
class EnvTest : public testing::Test {
 public:
  EnvTest() : env_(Env::Default()) {}
  Env* env_;
};

// TODO
TEST_F(EnvTest, ReadWrite){
  Random rnd(test::RandomSeed());

}

}  // namespace leveldb
