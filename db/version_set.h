#ifndef STORAGE_LEVELDB_DB_VERSION_SET_H_
#define STORAGE_LEVELDB_DB_VERSION_SET_H_

#include <map>
#include <set>
#include <vector>

#include "db/dbformat.h"
// #include "db/version_edit.h"
#include "port/port.h"
#include "port/thread_annotations.h"

namespace leveldb {
namespace log {
class Writer;
}  // namespace log

class Compaction;
class Iterator;
class MemTable;
class TableBuilder;
class TableCache;
class Version;
class VersionSet;
class WritableFile;

// 返回使 files[i]->largest >= key 的最小索引 i。
// 如果没有这样的文件，则返回 files.size()。
// 要求："files" 包含一个排序的、无重叠的文件列表。
int FindFile(const InternalKeyComparator& icmp,
             const std::vector<FileMetaData*>& file, const Slice& key);

// 当且仅当"files"中的某个文件与用户键范围[*smallest, *largest]重叠时返回true。
// smallest==nullptr 表示一个比数据库中所有键都小的键。
// largest==nullptr 表示一个比数据库中所有键都大的键。
// 要求：如果 disjoint_sorted_files 为 true，则 files[]
// 包含按排序顺序排列的不相交范围。
bool SomeFileOverlapsRange(const InternalKeyComparator& icmp,
                           bool disjoint_sorted_files,
                           const std::vector<FileMetaData*>& files,
                           const Slice* smallest_user_key,
                           const Slice* largest_user_key);

class Version {
 public:
  struct GetStats {
    FileMetaData* seek_file;
    int seek_file_level;
  };
  // 向 *iters 追加一系列迭代器，这些迭代器在合并后将生成此版本的内容。
  // 要求：此版本已保存（参见 VersionSet::SaveTo）
  void AddIterators(const ReadOptions& options, std::vector<Iterator*>* iters);

  // 查找键的值。如果找到，将其存储在*val中并返回OK。否则返回非OK状态。填充*stats。
  // 要求：未持有锁
  Status Get(const ReadOptions& options, const LookupKey& key, std::string* val,
             GetStats* stats);

  // 将"stats"添加到当前状态中。如果可能需要触发新的压缩，则返回true，否则返回false。
  // 要求：持有锁
  bool UpdateStats(const GetStats& stats);

  // 记录在指定内部键处读取的字节样本。
  // 大约每读取config::kReadBytesPeriod字节进行一次采样。如果可能需要触发新的压缩，则返回true。
  // 要求：持有锁
  bool RecordReadSample(Slice key);

  // Reference count management (so Versions do not disappear out from
  // under live iterators)
  void Ref();
  void Unref();

  void GetOverlappingInputs(int level, const InternalKey* begin,
                            const InternalKey* end,
                            std::vector<FileMetaData*>* inputs);
  // 当且仅当指定层级中的某个文件与 [*smallest_user_key, *largest_user_key]
  // 的某部分重叠时返回 true。
  // smallest_user_key==nullptr表示一个比数据库中所有键都小的键。
  // largest_user_key==nullptr表示一个比数据库中所有键都大的键。
  bool OverlapInLevel(int level, const Slice* smallest_user_key,
                      const Slice* largest_user_key);

  // 返回我们应该放置覆盖范围 [smallest_user_key, largest_user_key] 的新
  // memtable 压缩结果的层级。
  int PickLevelForMemTableOutput(const Slice& smallest_user_key,
                                 const Slice& largest_user_key);

  int NumFiles(int level) const { return files_[level].size(); }

  // Return a human readable string that describes this version's contents.
  std::string DebugString() const;

 private:
  friend class Compaction;
  friend class VersionSet;
  class LevelFileNumIterator;

  explicit Version(VersionSet* vset)
      : vset_(vset),
        next_(this),
        prev_(this),
        refs_(0),
        file_to_compact_(nullptr),
        file_to_compact_level_(-1),
        compaction_level_(-1),
        compaction_score_(-1) {}

  Version(const Version&) = delete;
  Version& operator=(const Version&) = delete;

  ~Version();

  Iterator* NewConcatenatingIterator(const ReadOptions& options,
                                     int level) const;
  // 对每个与 user_key 重叠的文件按从新到旧的顺序调用 func(arg, level, f)。
  // 如果某次调用 func 返回 false，则不再进行更多调用。
  //
  // 要求：internal_key 的用户部分等于 user_key。
  void ForEachOverlapping(Slice user_key, Slice internal_key, void* arg,
                          bool (*func)(void*, int, FileMetaData*));

  VersionSet* vset_;
  Version* next_;
  Version* prev_;
  int refs_;  // 对此版本的活动引用数量

  std::vector<FileMetaData*> files_[config::kNumLevels];

  FileMetaData* file_to_compact_;
  int file_to_compact_level_;

  // 下一个应该进行压缩的层级及其压缩得分。
  // 得分 < 1 表示不严格需要压缩。这些字段由 Finalize() 初始化。
  double compaction_score_;
  int compaction_level_;
};

// TODO
class VersionSet {
 public:
  VersionSet(const std::string& dbname, const Options* options,
             TableCache* table_cache, const InternalKeyComparator*);
  VersionSet(const VersionSet&) = delete;
  VersionSet& operator=(const VersionSet&) = delete;

  ~VersionSet();

  // 将 *edit 应用到当前版本以形成一个新的描述符，
  // 该描述符既保存到持久状态，又安装为新的当前版本。
  // 实际写入文件时将释放 *mu。
  // 要求：进入时持有 *mu。
  // 要求：没有其他线程同时调用 LogAndApply()
  Status LogAndApply(VersionEdit* edit, port::Mutex* mu)
      EXCLUSIVE_LOCKS_REQUIRED(mu);

  // 从持久存储中恢复最后保存的描述符。
  Status Recover(bool* save_manifest);

  // 返回当前版本。
  Version* current() const { return current_; }

  // 返回当前的清单文件编号。
  uint64_t ManifestFileNumber() const { return manifest_file_number_; }

  // 分配并返回一个新的文件编号。
  uint64_t NewFileNumber() { return next_file_number_++; }

  // 安排重用 "file_number"，除非已经分配了更新的文件编号。
  // 要求："file_number" 是通过调用 NewFileNumber() 返回的。
  void ReuseFileNumber(uint64_t file_number) {
    if (next_file_number_ == file_number + 1) {
      next_file_number_ = file_number;
    }
  }

  // 返回指定层级的表文件数量。
  int NumLevelFiles(int level) const;

  // 返回指定层级的所有文件的总大小。
  int64_t NumLevelBytes(int level) const;

  // 返回最后的序列号。
  uint64_t LastSequence() const { return last_sequence_; }

  // 将最后的序列号设置为 s。
  void SetLastSequence(uint64_t s) {
    assert(s >= last_sequence_);
    last_sequence_ = s;
  }

  // 标记指定的文件编号为已使用。
  void MarkFileNumberUsed(uint64_t number);

  // 返回当前的日志文件编号。
  uint64_t LogNumber() const { return log_number_; }

  // 返回当前正在压缩的日志文件的编号，如果没有这样的日志文件，则返回零。
  uint64_t PrevLogNumber() const { return prev_log_number_; }

  // 选择新的压缩的层级和输入。
  // 如果没有需要完成的压缩，则返回 nullptr。
  // 否则返回一个描述压缩的堆分配对象的指针。调用者应删除结果。
  Compaction* PickCompaction();

  // 返回一个压缩对象，用于压缩指定层级中的 [begin,end] 范围。
  // 如果该层级中没有与指定范围重叠的内容，则返回 nullptr。调用者应删除结果。
  Compaction* CompactRange(int level, const InternalKey* begin,
                           const InternalKey* end);

  // 返回在下一级别中任何文件的最大重叠数据（以字节为单位），适用于级别 >= 1。
  int64_t MaxNextLevelOverlappingBytes();

  // 创建一个迭代器，用于读取 "*c" 的压缩输入。
  // 当不再需要时，调用者应删除迭代器。
  Iterator* MakeInputIterator(Compaction* c);

  // 当且仅当某个层级需要压缩时返回 true。
  bool NeedsCompaction() const {
    Version* v = current_;
    return (v->compaction_score_ >= 1) || (v->file_to_compact_ != nullptr);
  }

  // 将任何活动版本中列出的所有文件添加到 *live。
  // 也可能会改变一些内部状态。
  void AddLiveFiles(std::set<uint64_t>* live);

  // 返回版本 "v" 中 "key" 的数据在数据库中的近似偏移量。
  uint64_t ApproximateOffsetOf(Version* v, const InternalKey& key);

  // 返回每个层级文件数量的可读简短（单行）摘要。使用 *scratch 作为后备存储。
  struct LevelSummaryStorage {
    char buffer[100];
  };
  const char* LevelSummary(LevelSummaryStorage* scratch) const;

 private:
  class Builder;
  friend class Compaction;
  friend class Version;

  bool ReuseManifest(const std::string& dscname, const std::string& dscbase);

  void Finalize(Version* v);

  void GetRange(const std::vector<FileMetaData*>& inputs, InternalKey* smallest,
                InternalKey* largest);

  void GetRange2(const std::vector<FileMetaData*>& inputs1,
                 const std::vector<FileMetaData*>& inputs2,
                 InternalKey* smallest, InternalKey* largest);

  void SetupOtherInputs(Compaction* c);

  Status WriteSnapshot(log::Writer* log);

  void AppendVersion(Version* v);

  Env* const env_;
  const std::string dbname_;
  const Options* const options_;
  TableCache* const table_cache_;
  const InternalKeyComparator icmp_;
  uint64_t next_file_number_;
  uint64_t manifest_file_number_;
  uint64_t last_sequence_;
  uint64_t log_number_;
  uint64_t prev_log_number_;  // 0 or backing store for memtable being compacted

  WritableFile* descriptor_file_;
  log::Writer* descriptor_log_;
  Version dummy_version_;  // 版本的循环双向链表的头部。
  Version* current_;       // == dummy_versions_.prev_

  // 每个层级的下一个压缩应从该层级的哪个键开始。
  // 可以是一个空字符串，或者是一个有效的 InternalKey。
  std::string compact_pointer_[config::kNumLevels];
};

// Compaction 类封装了有关压缩的信息。
class Compaction {
 public:
  ~Compaction();

  // 返回正在压缩的层级。"level"和"level+1"的输入将被合并以生成一组"level+1"文件。
  int level() const { return level_; }

  // 返回保存此压缩所做的描述符编辑的对象。
  VersionEdit* edit() { return &edit_; }

  // "which" must be either 0 or 1
  int num_input_files(int which) const { return inputs_[which].size(); }

  // 返回 "level()+which" 处的第 i 个输入文件（"which" 必须是 0 或 1）。
  FileMetaData* input(int which, int i) const { return inputs_[which][i]; }

  // 在此压缩过程中生成的文件的最大大小。
  uint64_t MaxOutputFileSize() const { return max_output_file_size_; }

  // 这是否是一个可以通过仅将单个输入文件移动到下一级别（无需合并或拆分）来实现的简单压缩？
  bool IsTrivialMove() const;

  // 将此压缩的所有输入作为删除操作添加到 *edit。
  void AddInputDeletions(VersionEdit* edit);

  // 如果我们现有的信息保证压缩在"level+1"生成的数据在高于"level+1"的层级中不存在，则返回true。
  bool IsBaseLevelForKey(const Slice& user_key);

  // 当且仅当我们应该在处理"internal_key"之前停止构建当前输出时返回true。
  bool ShouldStopBefore(const Slice& internal_key);

  // 一旦压缩成功，释放压缩的输入版本。
  void ReleaseInputs();

 private:
  friend class Version;
  friend class VersionSet;

  Compaction(const Options* options, int level);

  int level_;
  uint64_t max_output_file_size_;
  Version* input_version_;
  VersionEdit edit_;
  // 每次压缩从 "level_" 和 "level_+1" 读取输入
  std::vector<FileMetaData*> inputs_[2];
  // 用于检查重叠祖父文件数量的状态
  // （父级 == level_ + 1，祖父级 == level_ + 2）
  std::vector<FileMetaData*> grandparents_;
  size_t grandparent_index_;  // Index in grandparent_starts_ ?
  bool seen_key_;             // Some output key has been seen
  int64_t overlapped_bytes_;  // Bytes of overlap between current output
                              // and grandparent files

  // 实现 IsBaseLevelForKey 的状态
  // level_ptrs_ 保存了 input_version_->levels_ 的索引：我们的状态是
  // 我们定位在每个比当前压缩涉及的层级更高的文件范围内（即对于所有 L >= level_
  // + 2）。
  size_t level_ptrs_[config::kNumLevels];
};
}  // namespace leveldb

#endif