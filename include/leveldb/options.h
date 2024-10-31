#ifndef STORAGE_LEVELDB_INCLUDE_OPTIONS_H_
#define STORAGE_LEVELDB_INCLUDE_OPTIONS_H_

#include <cstddef>

#include "leveldb/export.h"

namespace leveldb {
class Cache;
class Comparator;
class Env;
class FilterPolicy;
class Logger;
class Snapshot;  // TODO

// DB contents are stored in a set of blocks, each of which holds a
// sequence of key,value pairs.  Each block may be compressed before
// being stored in a file.  The following enum describes which
// compression method (if any) is used to compress a block.
enum CompressionType {
  kNoCompression = 0x0,
  kSnappyCompression = 0x1,
  kZstdCompression = 0x2,
};

// 控制数据库行为的选项(passed to DB::Open)
struct LEVELDB_EXPORT Options {
  // 默认选项
  Options();

  // 影响行为的参数

  // 定义key的顺序
  const Comparator* comparator;

  // 是否在数据库不存在时创建数据库
  bool create_if_missing = false;

  // 是否在数据库已存在时报错
  bool error_if_exists = false;

  // 如果为 true，程序将对其处理的数据进行积极的检查，
  // 并在检测到任何错误时提前停止。这可能会有意想不到的后果：
  // 例如，一个数据库条目的损坏可能会导致大量条目变得不可读，
  // 或者整个数据库无法打开。
  bool paranoid_checks = false;

  // 使用指定的对象与环境交互，
  // 例如读取/写入文件、安排后台工作等。
  // 默认值：Env::Default()
  Env* env;

  // 任何由数据库生成的内部进度/错误信息将被写入 info_log（如果它非空），
  // 否则将写入存储在与数据库内容相同目录中的文件。
  Logger* info_log = nullptr;

  // 影响性能的参数

  // 在将数据转换为排序的磁盘文件之前，
  // 在内存中累积的数据量（由磁盘上的未排序日志支持）。
  //
  // 较大的值可以提高性能，特别是在批量加载期间。
  // 内存中最多可以同时保留两个写缓冲区，
  // 因此您可能希望调整此参数以控制内存使用。
  // 此外，较大的写缓冲区将导致下次打开数据库时恢复时间更长。
  size_t write_buffer_size = 4 * 1024 * 1024;

  // 数据库可以使用的打开文件数量。如果您的数据库有大量工作集，
  // 您可能需要增加此值（每 2MB 工作集预算一个打开文件）。
  int max_open_files = 1000;

  // 控制块（用户数据存储在一组块中，块是从磁盘读取的单位）。

  // 如果非空，使用指定的缓存来存储块。
  // 如果为空，leveldb 将自动创建并使用一个 8MB 的内部缓存。
  Cache* block_cache = nullptr;

  // 每个块中打包的用户数据的大致大小。请注意，
  // 此处指定的块大小对应于未压缩的数据。
  // 如果启用了压缩，从磁盘读取的实际单位大小可能会更小。
  // 此参数可以动态更改。
  size_t block_size = 4 * 1024;

  // 用于键的增量编码的重启点之间的键的数量。
  // 此参数可以动态更改。大多数客户端应保持此参数不变。
  int block_restart_interval = 16;

  // Leveldb 将在写入文件达到此字节数后切换到新文件。
  // 大多数客户端应保持此参数不变。然而，如果您的文件系统在处理较大文件时更高效，
  // 您可以考虑增加此值。缺点是压缩时间更长，从而导致更长的延迟/性能波动。
  // 另一个增加此参数的原因可能是在最初填充大型数据库时。
  size_t max_file_size = 2 * 1024 * 1024;

  // 使用指定的压缩算法压缩块。此参数可以动态更改。
  //
  // 默认值：kSnappyCompression，提供轻量但快速的压缩。
  //
  // 在 Intel(R) Core(TM)2 2.4GHz 上，kSnappyCompression 的典型速度：
  //    ~200-500MB/s 压缩速度
  //    ~400-800MB/s 解压速度
  // 请注意，这些速度显著快于大多数持久存储的速度，因此通常不值得切换到
  // kNoCompression。 即使输入数据不可压缩，kSnappyCompression
  // 实现也会高效地检测到这一点，并切换到未压缩模式。
  CompressionType compression = kSnappyCompression;

  // no use
  // Compression level for zstd.
  // Currently only the range [-5,22] is supported. Default is 1.
  int zstd_compression_level = 1;

  // EXPERIMENTAL：如果为 true，在打开数据库时附加到现有的 MANIFEST 和日志文件。
  // 这可以显著加快打开速度。
  //
  // 默认值：目前为 false，但以后可能会变为 true。
  bool reuse_logs = false;

  // 如果非空，使用指定的过滤策略来减少磁盘读取。
  // 许多应用程序将受益于在此传递 NewBloomFilterPolicy() 的结果。
  const FilterPolicy* filter_policy = nullptr;
};

// 控制数据库读取操作的选项
struct ReadOptions {
  // 如果为 true，从底层存储读取的所有数据都将与相应的校验和进行验证。
  bool verify_checksums = false;

  // 读取的数据是否应缓存在内存中？
  // 对于批量扫描，调用者可能希望将此字段设置为 false。
  bool fill_cache = true;

  // 如果 "snapshot" 非空，则根据提供的快照进行读取
  // （该快照必须属于正在读取的数据库，并且不能已被释放）。
  // 如果 "snapshot" 为空，则使用此读取操作开始时的隐式快照。
  const Snapshot* snapshot = nullptr;
};

// 控制数据库读取操作的选项
struct LEVELDB_EXPORT WriteOptions {

  // 如果为 true，在写入操作被认为完成之前，
  // 将通过调用 WritableFile::Sync() 从操作系统缓冲区缓存中刷新写入。
  // 如果此标志为 true，写入速度将会变慢。
  //
  // 如果此标志为 false，并且机器崩溃，可能会丢失一些最近的写入。
  // 请注意，如果只是进程崩溃（即机器没有重启），即使 sync==false
  // 也不会丢失任何写入。
  //
  // 换句话说，sync==false 的数据库写入具有类似于 "write()" 系统调用的崩溃语义。
  // sync==true 的数据库写入具有类似于 "write()" 系统调用后跟 "fsync()"
  // 的崩溃语义。
  bool sync = false;
};

}  // namespace leveldb

#endif