
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"
#include "leveldb/env.h"
#include "port/port.h"
#include "util/env_posix_test_helper.h"
#include "util/testutil.h"

namespace leveldb {
static const int kReadOnlyFileLimit = 4;
static const int kMMapLimit = 4;

class EnvPosixTest : public testing::Test {
 public:
  static void SetFileLimits(int read_only_file_limit, int mmap_limit) {
    EnvPosixTestHelper::SetReadOnlyFDLimit(read_only_file_limit);
    EnvPosixTestHelper::SetReadOnlyMMapLimit(mmap_limit);
  }

  EnvPosixTest() : env_(Env::Default()) {}
  Env* env_;
};

TEST_F(EnvPosixTest, TestOpenOnRead) {
  std::string test_dir;
  ASSERT_LEVELDB_OK(env_->GetTestDirectory(&test_dir));
  std::string test_file = test_dir + "/open_on_read.txt";

  std::FILE* f = std::fopen(test_file.c_str(), "we");
  ASSERT_TRUE(f != nullptr);
  const char kFileData[] = "abcdefghijklmnopqrstuvwxyz";
  fputs(kFileData, f);
  std::fclose(f);

  // Open test file some number above the sum of the two limits to force
  // open-on-read behavior of POSIX Env leveldb::RandomAccessFile.
  // 当已打开的文件超过上限时，RandomAccessFile会仅在使用时打开文件，其他时候fd是关闭的
  const int kNumFiles = kReadOnlyFileLimit + kMMapLimit + 5;
  leveldb::RandomAccessFile* files[kNumFiles] = {0};
  for (int i = 0; i < kNumFiles; i++) {
    ASSERT_LEVELDB_OK(env_->NewRandomAccessFile(test_file, &files[i]));
  }
  char scratch;
  Slice read_result;
  for (int i = 0; i < kNumFiles; i++) {
    ASSERT_LEVELDB_OK(files[i]->Read(i, 1, &read_result, &scratch));
    ASSERT_EQ(kFileData[i], read_result[0]);
  }
  for (int i = 0; i < kNumFiles; i++) {
    delete files[i];
  }
  ASSERT_LEVELDB_OK(env_->RemoveFile(test_file));
}

}  // namespace leveldb

int main(int argc, char** argv) {
  leveldb::EnvPosixTest::SetFileLimits(leveldb::kReadOnlyFileLimit,
                                       leveldb::kMMapLimit);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}