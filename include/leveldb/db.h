#ifndef STORAGE_LEVELDB_INCLUDE_DB_H_
#define STORAGE_LEVELDB_INCLUDE_DB_H_

#include <cstdint>
#include <cstdio>

#include "leveldb/export.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"

namespace leveldb {
// 版本号与CMakeLists.txt保持一致
static const int kMajorVersion = 0;
static const int kMinorVersion = 1;

struct Options;
struct ReadOptions;
struct WriteOptions;
class WriteBatch;

class LEVELDB_EXPORT Snapshot {
 protected:
  virtual ~Snapshot();
};

// [Start,limit)
struct LEVELDB_EXPORT Range {
  Range() = default;
  Range(const Slice& s, const Slice& l) : start(s), limit(l) {}

  Slice start;
  Slice limit;
};

// DB 是一个从键到值的持久有序映射。
// DB 对多个线程的并发访问是安全的，无需任何外部同步。
class LEVELDB_EXPORT DB {
 public:
  // 打开指定名称的数据库。
  // 在 *dbptr 中存储一个堆分配的数据库指针，并在成功时返回 OK。
  // 在 *dbptr 中存储 nullptr，并在出错时返回非 OK 状态。
  // 当不再需要时，调用者应删除 *dbptr。
  static Status Open(const Options& options, const std::string& name,
                     DB** dbptr);

  DB() = default;
  DB(const DB&) = delete;
  DB& operator=(const DB&) = delete;

  virtual ~DB();

  // 将数据库条目设置为 "key" 对应的 "value"。成功时返回 OK，出错时返回非 OK
  // 状态。 注意：考虑将 options.sync 设置为 true。
  virtual Status Put(const WriteOptions& options, const Slice& key,
                     const Slice& value) = 0;

  // 移除 "key" 对应的数据库条目（如果有）。成功时返回 OK，出错时返回非 OK
  // 状态。 如果 "key" 在数据库中不存在，这不是一个错误。 注意：考虑将
  // options.sync 设置为 true。
  virtual Status Delete(const WriteOptions& options, const Slice& key) = 0;

  // 将指定的更新应用到数据库。
  // 成功时返回 OK，失败时返回非 OK 状态。
  // 注意：考虑将 options.sync 设置为 true。
  virtual Status Write(const ReadOptions& options, WriteBatch* updates) = 0;

  // 如果数据库包含 "key" 的条目，则将相应的值存储在 *value 中并返回 OK。
  // 如果没有 "key" 的条目，则保持 *value 不变并返回一个 Status::IsNotFound()
  // 返回 true 的状态。
  // 出错时可能返回其他状态。
  virtual Status Get(const ReadOptions& options, const Slice& key,
                     std::string* value) = 0;

  // 返回一个堆分配的迭代器，用于遍历数据库的内容。
  // NewIterator() 的结果最初是无效的（调用者必须在使用迭代器之前调用其中一个
  // Seek 方法）。
  //
  // 当不再需要迭代器时，调用者应删除它。
  // 返回的迭代器应在删除此数据库之前删除。
  virtual Iterator* NewIterator(const ReadOptions& options) = 0;

  // 返回当前数据库状态的句柄。使用此句柄创建的迭代器将观察当前数据库状态的稳定快照。
  // 当不再需要快照时，调用者必须调用 ReleaseSnapshot(result)。
  virtual const Snapshot* GetSnapshot() = 0;

  // 释放先前获取的快照。调用此函数后，调用者不得再使用 "snapshot"。
  virtual void ReleaseSnapshot(const Snapshot* snapshot) = 0;

  // DB 实现可以通过此方法导出有关其状态的属性。
  // 如果 "property" 是此 DB 实现理解的有效属性，则用其当前值填充 "*value"
  // 并返回 true。 否则返回 false。
  //
  // 有效的属性名称包括：
  //
  //  "leveldb.num-files-at-level<N>" - 返回第 <N> 层的文件数量，
  //     其中 <N> 是层级编号的 ASCII 表示（例如 "0"）。
  //  "leveldb.stats" - 返回一个多行字符串，描述有关 DB 内部操作的统计信息。
  //  "leveldb.sstables" - 返回一个多行字符串，描述组成数据库内容的所有
  //  sstables。 "leveldb.approximate-memory-usage" - 返回 DB
  //  使用的大致内存字节数。
  virtual bool GetProperty(const Slice& property, std::string* value) = 0;

  // 对于 [0,n-1] 中的每个 i，将 "[range[i].start .. range[i].limit)"
  // 中键使用的大致文件系统空间存储在 "sizes[i]" 中。
  //
  // 请注意，返回的大小衡量的是文件系统空间使用情况，
  // 因此如果用户数据压缩了十倍，返回的大小将是相应用户数据大小的十分之一。
  //
  // 结果可能不包括最近写入数据的大小。
  virtual void GetApproximateSizes(const Range* range, int n,
                                   uint64_t* sizes) = 0;

  // 压缩键范围 [*begin,*end] 的底层存储。
  // 特别是，删除和覆盖的版本将被丢弃，数据将被重新排列以减少访问数据所需的操作成本。
  // 此操作通常应仅由了解底层实现的用户调用。
  //
  // begin==nullptr 被视为在数据库中所有键之前的键。
  // end==nullptr 被视为在数据库中所有键之后的键。
  // 因此，以下调用将压缩整个数据库：
  //    db->CompactRange(nullptr, nullptr);
  virtual void CompactRange(const Slice* begin, const Slice* end) = 0;
};

// ?
// 销毁指定数据库的内容。
// 使用此方法时要非常小心。
//
// 注意：为了向后兼容，如果 DestroyDB 无法列出数据库文件，仍将返回
// Status::OK()，掩盖此失败。
LEVELDB_EXPORT Status DestoryDB(const std::string& name,
                                const Options& options);

// 如果无法打开数据库，您可以尝试调用此方法尽可能恢复数据库的内容。
// 可能会丢失一些数据，因此在对包含重要信息的数据库调用此函数时要小心。
LEVELDB_EXPORT Status RepairDB(const std::string& dbname,
                               const Options& options);
}  // namespace leveldb

#endif