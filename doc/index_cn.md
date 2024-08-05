leveldb
=======

_Jeff Dean, Sanjay Ghemawat_

**leveldb**提供了键值对的持久存储。键和值都可以是任意的字节数组。键根据用户指定的比较函数进行排序。

## 打开数据库
一个**levledb**数据库对应一个文件系统目录。数据库的所有内容都保存在这个目录中。下面的示例展示了如何创建（如果不存在）和打开数据库：

```cpp
#include <cassert>
#include "leveldb/db.h"

leveldb::DB* db;
leveldb::Options options;
options.create_if_missing = true; // 数据库不存在则创建
leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db); //"/tmp/testdb"为数据库存放目录
assert(status.ok());
...
```

如果您希望程序在发现数据库存在时报错，那么请在执行`leveldb::DB::Open`方法前添加下面的设置：

```cpp
options.error_if_exists = true;
```

## 状态 (Status)

您可能已经注意到上面的`leveldb::Status`类型了。**leveldb**中大多数可能遇到错误的函数都会返回这种类型的值。您可以检查这样的结果是否正确，并打印相关的错误消息：

```cpp
leveldb::Status s = ...;
if (!s.ok()) cerr << s.ToString() << endl;
```

## 关闭数据库

当您完成数据库相关的任务时，只需要简单地`delete`掉数据库的对象。示例：

```cpp
... open the db as described above ...
... do something with db ...
delete db;
```

## 数据库读写

数据库提供了`Put`，`Delete`，`Get`方法来修改/查询数据库。下面的代码示例展示了如何将`key1`对应的值更新为与`key2`对应

```cpp
std::string value;
leveldb::Status s = db->Get(leveldb::ReadOptions(), key1, &value);
if (s.ok()) s = db->Put(leveldb::WriteOptions(), key2, value);
if (s.ok()) s = db->Delete(leveldb::WriteOptions(), key2);
```

## 原子更新

注意到：在上面的例子中，如果程序在`Put`和`Delete`函数之间终止，那么相同的值可能会对应多个不同的`key`。这样的问题可以通过使用`WriteBatch`类来避免。`WriteBatch`类可以原子地执行一系列的更新操作：

```cpp
#include "levledb/write_batch.h"
...
std::string value;
leveldb:Status s = db->Get(leveldb::ReadOptions(), key1, &value);
if (s.ok()) {
    leveldb::WriteBatch batch;
    batch.Delete(key1);
    batch.Put(key2, value);
    s = db->Write(leveldb::WriteOptions(),&batch);
}
```

`WriteBatch`类有序保存了一系列对数据库的编辑。注意到我们在`Put`函数前执行了`Delete`，所以如果key1与key2相同，我们也不会错误地从数据库中彻底地删除掉该值

除了原子性的好处之外，您可以通过使用`WriteBatch`类，将多个独立的变化放入到同一个`batch`中，从而加快批量更新

## 同步写入

默认情况下，对leveldb的每次写入都是异步的：写入函数在将写入操作从进程推送到操作系统后便会返回。从操作系统内存到底层持久存储的传输是异步进行的。可以为特定写入打开同步标志，以使写入操作在写入的数据一直推送到永久存储之前不会返回。（在Posix系统上，这是通过在写入操作返回之前调用`fsync(...)`或者`fdatasync(...)`或者`msync(..., MS_SYNC)` 实现的。）

```cpp
leveldb::WriteOptions write_options;
write_options.sync = true;
db->Put(write_options, ...);
```

异步写入的速度通常是同步写入的一千倍以上。异步写入的缺点是，机器崩溃可能会导致最后几次更新丢失。请注意，仅写入进程崩溃（即，非重新启动类型的错误）不会造成任何数据上的丢失。因为即使关闭了同步，进程会先将更新从进程内存推送到操作系统中，然后才考虑操作是否完成。

`WriteBatch`类提供了异步写入的替换方案。多个使用同步写入的更新可以放置在同一WriteBatch中（即write_options.sync设置为true）。同步写入的额外成本将分摊在批处理中的所有写入中。

## 并发

一个数据库一次只能由一个进程打开。leveldb实现从操作系统获取锁以防止误用。在单个进程中，多个并发线程可以安全地共享同一个`leveldb::DB`对象。也就是说，不同的线程可以在没有任何外部同步的情况下在同一个数据库上写入或获取迭代器或调用Get（leveldb实现将自动进行所需的同步）。但是，其他对象（如Iterator和`WriteBatch`）可能需要外部同步。如果两个线程共享这样一个对象，它们必须使用自己的锁定协议来保护对该对象的访问。公共头文件中提供了更多详细信息。

## 迭代

下面的示例展示如何打印数据库中的所有键值对

```cpp
leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
for (it->SeekToFirst(); it->Valid(); it->Next()) {
  cout << it->key().ToString() << ": "  << it->value().ToString() << endl;
}
assert(it->status().ok());  // 检查查询数据时是否出错
delete it;
```

下面的变化展示了如何仅处理中`[start,limit)`中的`key`：

```cpp
for (it->Seek(start);
   it->Valid() && it->key().ToString() < limit;
   it->Next()) {
  ...
}
```

您也可以按相反的顺序处理条目。（注意：反向迭代可能比正向迭代慢一些。）

```cpp
for (it->SeekToLast(); it->Valid(); it->Prev()) {
  ...
}
```

## 快照

快照为键值存储的整个状态提供了一致的只读视图。`ReadOptions::snapshot`为`non-NULL`时表示读取应发生在特定版本的数据库状态上。如果`ReadOptions::snapshot`为`NULL`，则读取操作将对当前状态的隐式快照进行。

快照通过`DB::GetSnapshot()` 方法获得：

```cpp
leveldb::ReadOptions options;
options.snapshot = db->GetSnapshot();
... apply some updates to db ...
leveldb::Iterator* iter = db->NewIterator(options);
... read using iter to view the state when the snapshot was created ...
delete iter;
db->ReleaseSnapshot(options.snapshot);
```

请注意，当不再需要快照时，应使用`DB::ReleaseSnapshot`接口释放快照。这允许实现摆脱仅为支持读取快照而维护的状态

## 切片

上面的`it->key()`和`it->value()`的返回值是`leveldb::Slice`类型的实例。`Slice`是一个简单的结构，它包含一个长度和指向外部字节数组的指针。返回`Slice`是相较于返回`std::string`的而言一种更低开销的选择，因为我们不需要复制潜在的大键和值。此外，`leveldb`方法不返回以`null`结尾的`C`样式字符串，因为leveldb允许`键`和`值`包含`'\0'`字节。

`C++`字符串和以`null`结尾的`C`样式字符串可以很容易地转换为`Slice`：

```cpp
leveldb::Slice s1 = "hello";

std::string str("world");
leveldb::Slice s2 = str;
```

`Slice`可以很容易地转换回`C++`字符串：

```cpp
std::string str = s1.ToString();
assert(str == std::string("hello"));
```

注意：调用方需要确保`Slice`所指向的外部字节数组在使用`Slice`期间保持存活。例如，以下用法存在`bug`：

```cpp
leveldb::Slice slice;
if (...) {
  std::string str = ...;
  slice = str;
}
Use(slice);
```

当if语句执行结束时，`str`将被销毁，slice的存储的对象也将消失。

## 比较器

前面的示例使用了key的默认排序函数，该函数按字典顺序排列字节。但是，您可以在打开数据库时提供自定义比较器。例如：假设每个数据库键由两个数字组成，我们想要先按第一个数字排序，再在第一个数据相等时用第二个数字来排序。首先，定义`leveldb::Comparator`的一个适当的子类，该子类表示以下规则：

```cpp
class TwoPartComparator : public leveldb::Comparator {
 public:
  // Three-way comparison function:
  //   if a < b: negative result
  //   if a > b: positive result
  //   else: zero result
  int Compare(const leveldb::Slice& a, const leveldb::Slice& b) const {
    int a1, a2, b1, b2;
    ParseKey(a, &a1, &a2);
    ParseKey(b, &b1, &b2);
    if (a1 < b1) return -1;
    if (a1 > b1) return +1;
    if (a2 < b2) return -1;
    if (a2 > b2) return +1;
    return 0;
  }

  // Ignore the following methods for now:
  const char* Name() const { return "TwoPartComparator"; }
  void FindShortestSeparator(std::string*, const leveldb::Slice&) const {}
  void FindShortSuccessor(std::string*) const {}
};
```

现在使用这个自定义比较器创建数据库：

```cpp
TwoPartComparator cmp;
leveldb::DB* db;
leveldb::Options options;
options.create_if_missing = true;
options.comparator = &cmp;
leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
...
```

### 向后兼容

比较器的`Name`方法的结果会在数据库创建时附加到其上，并在随后打开的每个数据库中进行检查。如果名称发生更改，`leveldb::DB::Open`调用将失败。因此，当且仅当新的`key`格式和比较函数与现有数据库不兼容时，才可以更改名称，并且可以丢弃所有现有数据库的内容。

然而，随着时间的推移，您仍然可以通过一些预先规划来逐步发展您的`key`格式。例如，您可以在每个键的末尾存储一个版本号（一个字节应该足以满足大多数用途）。当您希望切换到新的`key`格式（例如，向`TwoPartComparator`处理的`key`添加可选的第三部分）时，`（a）`保持相同的比较器名称（b）增加新`key`的版本号（c）更改比较器功能，使其使用`key`中的版本号来决定如何解释它们。

## 性能表现

可以通过更改`include/options.h`中定义的类型的默认值来调整性能。

### 块大小

`leveldb`将相邻的`key`分组到同一个块中，这样的块是向持久存储传输和从持久存储传输的单位。默认块大小约为4096个未压缩字节。主要对数据库内容进行批量扫描的应用程序可能希望增加此大小。那么对小值进行大量点读取的应用程序可能希望切换到较小的块大小从而改进性能。使用小于`1k`字节或大于几兆字节的块并没有多大好处。另外还需要注意，块大小越大，压缩效果就越好。

### 压缩

每个块在被写入永久存储器之前都会被单独压缩。默认情况下，压缩处于启用状态，因为默认压缩方法非常快，并且会自动禁用不可压缩数据。在极少数情况下，应用程序可能希望完全禁用压缩，但只有在基准测试显示性能有所提高时才应该这样做

```cpp
leveldb::Options options;
options.compression = leveldb::kNoCompression;
... leveldb::DB::Open(options, name, ...) ....
```

### 缓存

数据库的内容存储在文件系统中的一组文件中，每个文件存储一系列压缩块。options.block_cache`设置为`non-NULL`常用于缓存常用的未压缩块内容。

```cpp
#include "leveldb/cache.h"

leveldb::Options options;
options.block_cache = leveldb::NewLRUCache(100 * 1048576);  // 100MB cache
leveldb::DB* db;
leveldb::DB::Open(options, name, &db);
... use the db ...
delete db
delete options.block_cache;
```

请注意，缓存保存未压缩的数据，因此应根据应用程序级别的数据大小对其进行调整，而不需要对压缩设置进行调整。（压缩块的缓存留给操作系统缓冲区缓存或客户端提供的任何自定义环境实现。）
当执行大容量读取时，应用程序可能希望禁用缓存，以便大容量读取处理的数据不会最终取代大部分缓存内容。可以使用每个迭代器选项来实现这一点：

```cpp
leveldb::ReadOptions options;
options.fill_cache = false;
leveldb::Iterator* it = db->NewIterator(options);
for (it->SeekToFirst(); it->Valid(); it->Next()) {
  ...
}
delete it;
```

### key布局

请注意，磁盘传输和缓存的单位是一个块。相邻的`key`（根据数据库排序顺序）通常会放在同一个块中。因此，应用程序可以通过将一起访问的`key`放置在彼此靠近的位置，并将不常用的`key`放置到单独区域来提高其性能。

例如，假设我们在`leveldb`之上实现一个简单的文件系统。我们可能希望存储的条目类型有：

```
filename -> permission-bits, length, list of file_block_ids
file_block_id -> data
```

我们可能希望用一个字母（比如`'/'`）作为文件名键的前缀，用另一个字母来作为`file_block_id`键的前缀（比如`'0'`），这样仅扫描元数据就不会迫使我们获取和缓存庞大的文件内容。

### 过滤器

由于`leveldb`数据在磁盘上的组织方式，单个`Get()`调用可能涉及到从磁盘进行多次读取。可选的`FilterPolicy`机制可用于大幅减少磁盘读取次数。

```cpp
leveldb::Options options;
options.filter_policy = NewBloomFilterPolicy(10);
leveldb::DB* db;
leveldb::DB::Open(options, "/tmp/testdb", &db);
... use the database ...
delete db;
delete options.filter_policy;
```

前面的代码将基于`Bloom`过滤器的过滤策略与数据库相关联。基于`Bloom`过滤器的过滤依赖于每个`key`在内存中的一定数量的数据位（在这个示例中下，每个`key`10位，因为这是我们传递给NewBloomFilterPolicy的参数）。此筛选器将把`Get()`调用所需的不必要的磁盘读取次数减少大约100倍。增加每个`key`的比特数将以更多的内存使用为代价导致更大的读取次数的减少。我们建议那些工作数据不适合存储在内存中而且需要进行大量随机读取的应用程序设置筛选策略。

如果使用自定义比较器，则应确保使用的筛选器策略与比较器兼容。例如，考虑一个比较器，它在比较键时忽略尾部空格。`NewBloomFilterPolicy`不能与此类比较器一起使用。相反，应用程序应该提供一个自定义的过滤器策略，该策略也会忽略尾部空格。例如：

```cpp
class CustomFilterPolicy : public leveldb::FilterPolicy {
 private:
  leveldb::FilterPolicy* builtin_policy_;

 public:
  CustomFilterPolicy() : builtin_policy_(leveldb::NewBloomFilterPolicy(10)) {}
  ~CustomFilterPolicy() { delete builtin_policy_; }

  const char* Name() const { return "IgnoreTrailingSpacesFilter"; }

  void CreateFilter(const leveldb::Slice* keys, int n, std::string* dst) const {
    // Use builtin bloom filter code after removing trailing spaces
    std::vector<leveldb::Slice> trimmed(n);
    for (int i = 0; i < n; i++) {
      trimmed[i] = RemoveTrailingSpaces(keys[i]);
    }
    builtin_policy_->CreateFilter(trimmed.data(), n, dst);
  }
};
```

高级应用程序可能会提供不使用`Bloom`过滤器而使用某些其他机制来汇总一组key的过滤器策略。有关详细信息，请参见`leveldb/filter_policy.h`。

## 校验和

`leveldb`将校验和与存储在文件系统中的所有数据相关联。两个独立的控制配置控制这些校验和的验证强度：
`ReadOptions::verify_checksums`可以设置为`true`，以强制对代表特定读取从文件系统读取的所有数据进行校验和验证。默认情况下，不进行此类验证。
`Options::paranoid_checks`可以在打开数据库之前设置为`true`，以使数据库实现在检测到内部损坏时立即引发错误。根据数据库的哪一部分已损坏，打开数据库时或稍后通过另一个数据库操作可能会引发错误。默认情况下，`paranoid_checks`是关闭的。在这种情况下，即使数据库的部分持久存储已损坏，也可以使用数据库。
如果数据库已损坏（可能在启用偏执检查时无法打开），则可以使用`leveldb::RepairDB`函数来恢复尽可能多的数据

## 近似尺寸

`GetApproximateSizes`方法可用于获取一个或多个键范围使用的文件系统空间的大致字节数。

```cpp
leveldb::Range ranges[2];
ranges[0] = leveldb::Range("a", "c");
ranges[1] = leveldb::Range("x", "z");
uint64_t sizes[2];
db->GetApproximateSizes(ranges, 2, sizes);
```

上面的调用将把`size[0]`设置为key在`[a.c)`范围内使用的文件系统空间的大致字节数，把`size[1]`设置为key在`[x.z)`范围内使用的大致字节数。

## 环境

`leveldb`发出的所有文件操作（以及其他操作系统调用）都通过`leveldb::Env`对象进行路由。成熟的客户可能希望提供自己的Env实现，以获得更好的控制。例如，应用程序可能会在文件`IO`路径中引入人为延迟，以限制`leveldb`对系统中其他活动的影响。

```cpp
class SlowEnv : public leveldb::Env {
  ... implementation of the Env interface ...
};

SlowEnv env;
leveldb::Options options;
options.env = &env;
Status s = leveldb::DB::Open(options, ...);
```

## 移植

可以通过`leveldb/port/port.h`提供的特定于平台的实现导出的类型/方法/函数，将`leveldb`移植到新平台。有关更多详细信息，请参阅`leveldb/port_example.h`。
此外，新平台可能需要一个新的默认级别`db::Env`实现。有关示例，请参见`leveldb/util/env_posix.h`。

## 其他信息

有关`leveldb`实现的详细信息，请参阅以下文档：

1. [实现笔记](impl_cn.md)
2. [不可变表文件的格式](table_format_cn.md)
3. [日志文件的格式](log_format_cn.md)