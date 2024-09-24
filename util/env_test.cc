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
TEST_F(EnvTest, ReadWrite) {
  Random rnd(test::RandomSeed());

  // 获取用于测试的文件
  std::string test_dir;
  ASSERT_LEVELDB_OK(env_->GetTestDirectory(&test_dir));
  std::string test_file_name = test_dir + "/open_on_read.txt";
  WritableFile* writable_file;
  ASSERT_LEVELDB_OK(env_->NewWritableFile(test_file_name, &writable_file));

  // 写入测试
  static const size_t kDataSize = 10 * 1048576;
  std::string data;  // 后面会用
  while (data.size() < kDataSize) {
    int len = rnd.Skewed(18);
    std::string r;
    test::RandomString(&rnd, len, &r);
    ASSERT_LEVELDB_OK(writable_file->Append(r));
    data += r;
    if (rnd.OneIn(10)) {
      ASSERT_LEVELDB_OK(writable_file->Flush());
    }
  }
  ASSERT_LEVELDB_OK(writable_file->Sync());
  ASSERT_LEVELDB_OK(writable_file->Close());
  delete writable_file;

  // 顺序读
  SequentialFile* sequential_file;
  ASSERT_LEVELDB_OK(env_->NewSequentialFile(test_file_name, &sequential_file));
  std::string read_result;
  std::string scratch;
  while (read_result.size() < data.size()) {
    int len = std::min<int>(rnd.Skewed(18), data.size() - read_result.size());
    scratch.resize(std::max<int>(len, 1));
    Slice read;
    ASSERT_LEVELDB_OK(sequential_file->Read(
        len, &read, &scratch[0]));  // 强制写string的char数组数据
    if (len > 0) {
      ASSERT_GT(read.size(), 0);
    }
    ASSERT_LE(read.size(), len);
  }
  ASSERT_EQ(read_result, data);
  delete sequential_file;
}

TEST_F(EnvTest, RunImmediately) {
  struct RunState {
    port::Mutex mu;
    port::CondVar cvar{&mu};
    bool called = false;
    static void Run(void* arg) {
      RunState* state = reinterpret_cast<RunState*>(arg);
      MutexLock l(&state->mu);
      ASSERT_FALSE(state->called);
      state->called = true;
      state->cvar.Signal();
    }
  };

  RunState state;
  env_->Schedule(&RunState::Run, &state);
  // 测试阻塞
  MutexLock l(&state.mu);
  while (!state.called) {
    state.cvar.Wait();
  }
}

TEST_F(EnvTest, RunMany) {
  struct RunState {
    port::Mutex mu;
    port::CondVar cvar{&mu};
    int run_count = 0;
  };

  struct Callback {
    RunState* const state_;
    bool run = false;

    Callback(RunState* s) : state_(s) {}
    static void Run(void* arg) {
      Callback* callback = reinterpret_cast<Callback*>(arg);
      RunState* state = callback->state_;
      MutexLock l(&state->mu);
      state->run_count++;
      callback->run = true;
      state->cvar.Signal();
    }
  };

  RunState state;
  Callback callback1(&state);
  Callback callback2(&state);
  Callback callback3(&state);
  Callback callback4(&state);

  env_->Schedule(&Callback::Run, &callback1);
  env_->Schedule(&Callback::Run, &callback2);
  env_->Schedule(&Callback::Run, &callback3);
  env_->Schedule(&Callback::Run, &callback4);

  MutexLock l(&state.mu);
  while (state.run_count != 4) {
    state.cvar.Wait();
  }

  ASSERT_TRUE(callback1.run);
  ASSERT_TRUE(callback2.run);
  ASSERT_TRUE(callback3.run);
  ASSERT_TRUE(callback4.run);
}

struct State {
  port::Mutex mu;
  port::CondVar cvar{&mu};

  int val GUARDED_BY(mu);
  int num_running GUARDED_BY(mu);
  State(int val, int num_running) : val(val), num_running(num_running) {}
};

static void ThreadBody(void* arg) {
  State* s = reinterpret_cast<State*>(arg);
  MutexLock l(&s->mu);
  s->val += 1;
  s->num_running -= 1;
  s->cvar.Signal();
}

TEST_F(EnvTest, StartThread) {
  State state(0, 3);
  for (int i = 0; i < 3; i++) {
    env_->StartThread(&ThreadBody, &state);
  }
  MutexLock l(&state.mu);
  while (state.num_running != 0) {
    state.cvar.Wait();
  }
  ASSERT_EQ(state.val, 3);
}

TEST_F(EnvTest, TestOpenNonExistentFile) {
  std::string test_dir;
  ASSERT_LEVELDB_OK(env_->GetTestDirectory(&test_dir));
  std::string non_existent_file = test_dir + "/non_existent_file";
  ASSERT_FALSE(env_->FileExists(non_existent_file));
  RandomAccessFile* random_access_file;
  Status status =
      env_->NewRandomAccessFile(non_existent_file, &random_access_file);
  ASSERT_TRUE(status.IsNotFound());
  SequentialFile* sequential_file;
  status = env_->NewSequentialFile(non_existent_file, &sequential_file);
  ASSERT_TRUE(status.IsNotFound());
}

TEST_F(EnvTest, ReopenWritableFile) {
  std::string test_dir;
  ASSERT_LEVELDB_OK(env_->GetTestDirectory(&test_dir));
  std::string test_file_name = test_dir + "/reopen_writable_file.txt";
  env_->RemoveFile(test_file_name);
  WritableFile* wirtable_file;
  ASSERT_LEVELDB_OK(env_->NewWritableFile(test_file_name, &wirtable_file));
  std::string data("hello world!");
  ASSERT_LEVELDB_OK(wirtable_file->Append(Slice(data)));
  ASSERT_LEVELDB_OK(wirtable_file->Close());
  delete wirtable_file;

  ASSERT_LEVELDB_OK(env_->NewWritableFile(test_file_name, &wirtable_file));
  data = "42";
  ASSERT_LEVELDB_OK(wirtable_file->Append(Slice(data)));
  ASSERT_LEVELDB_OK(wirtable_file->Close());
  delete wirtable_file;

  ASSERT_LEVELDB_OK(ReadFileToString(env_, test_file_name, &data));
  ASSERT_EQ(std::string("42"), data);
  env_->RemoveFile(test_file_name);
}

TEST_F(EnvTest, ReopenAppendableFile) {
  std::string test_dir;
  ASSERT_LEVELDB_OK(env_->GetTestDirectory(&test_dir));
  std::string test_file_name = test_dir + "/reopen_writable_file.txt";
  env_->RemoveFile(test_file_name);
  WritableFile* wirtable_file;
  ASSERT_LEVELDB_OK(env_->NewAppendableFile(test_file_name, &wirtable_file));
  std::string data("hello world!");
  ASSERT_LEVELDB_OK(wirtable_file->Append(Slice(data)));
  ASSERT_LEVELDB_OK(wirtable_file->Close());
  delete wirtable_file;

  ASSERT_LEVELDB_OK(env_->NewAppendableFile(test_file_name, &wirtable_file));
  data = "42";
  ASSERT_LEVELDB_OK(wirtable_file->Append(Slice(data)));
  ASSERT_LEVELDB_OK(wirtable_file->Close());
  delete wirtable_file;

  ASSERT_LEVELDB_OK(ReadFileToString(env_, test_file_name, &data));
  ASSERT_EQ(std::string("hello world!42"), data);
  env_->RemoveFile(test_file_name);
}

}  // namespace leveldb
