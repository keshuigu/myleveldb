# Note 1 - 从`db_bench.cc`开始

## `main(int argc, char** argv)`

程序的执行入口`main`主要做了以下几件事情：

1. 设置`write_buffer_size`为4MB，该值控制内存中维护的数据缓冲大小
2. 设置`max_file_size`为2MB，该值控制leveldb存储文件的大小。leveldb会在文件超过该值后切换到新文件
3. 设置`block_size`为4KB，该值控制leveldb每个数据块的大小
4. 设置`max_open_files`为1000，该值控制最多由level控制的文件的数量
5. 根据输入的参数设置一系列全局值，暂时先不管
6. 根据当前操作系统设置`g_env`为默认的leveldb环境
7. 从`g_env`中获得测试时数据库文件的存放目录
8. 初始化`Benchmark`类，并开始执行测试

默认的`Benchmark`包含：`fillseq`, `fillsync`, `fillrandom`, `overwrite`, `readrandom`, `readrandom`, `readseq`, `readreverse`, `compact`, `readrandom`, `readseq`, `readreverse`, `fill100K`, `crc32c`, `snappycomp`, `snappyuncomp`, `zstdcomp`, `zstduncomp`,

## `Benchmark`类

### 成员变量

| 名称                | 类型                | 用途 |
| ------------------- | ------------------- | ---- |
| cache_              | Cache*              | TODO |
| filter_policy_      | const FilterPolicy* | TODO |
| db_                 | DB*                 | TODO |
| num_                | int                 | TODO |
| value_size_         | int                 | TODO |
| entries_per_batch_  | int                 | TODO |
| write_options_      | WriteOptions        | TODO |
| reads_              | int                 | TODO |
| heap_counter_       | int                 | TODO |
| count_comparator_   | CountComparator     | TODO |
| total_thread_count_ | int                 | TODO |

### 构造函数

```cpp
Benchmark()
      : cache_(FLAGS_cache_size >= 0 ? NewLRUCache(FLAGS_cache_size) : nullptr),
        filter_policy_(FLAGS_bloom_bits >= 0
                           ? NewBloomFilterPolicy(FLAGS_bloom_bits)
                           : nullptr),
        db_(nullptr),
        num_(FLAGS_num),
        value_size_(FLAGS_value_size),
        entries_per_batch_(1),
        reads_(FLAGS_reads < 0 ? FLAGS_num : FLAGS_reads),
        heap_counter_(0),
        count_comparator_(BytewiseComparator()),
        total_thread_count_(0) {
    std::vector<std::string> files;
    g_env->GetChildren(FLAGS_db, &files);
    for (size_t i = 0; i < files.size(); i++) {
      if (Slice(files[i]).starts_with("heap-")) {
        g_env->RemoveFile(std::string(FLAGS_db) + "/" + files[i]);
      }
    }
    if (!FLAGS_use_existing_db) {
      DestroyDB(FLAGS_db, Options());
    }
  }
```
简单的做了些清理工作

### `Run()`函数

```cpp
void Run() {
  PrintHeader(); // 打印环境信息
  Open(); // 打开数据库

  const char* benchmarks = FLAGS_benchmarks;

  // 执行每一个测试
  while (benchmarks != nullptr) {
    const char* sep = strchr(benchmarks, ',');
    Slice name;
    if (sep == nullptr) {
      name = benchmarks;
      benchmarks = nullptr;
    } else {
      name = Slice(benchmarks, sep - benchmarks);
      benchmarks = sep + 1;
    }

    // Reset parameters that may be overridden below
    num_ = FLAGS_num;
    reads_ = (FLAGS_reads < 0 ? FLAGS_num : FLAGS_reads);
    value_size_ = FLAGS_value_size;
    entries_per_batch_ = 1;
    write_options_ = WriteOptions();
    /**
     *声明了一个名为method的指针，这个指针指向Benchmark类的一个成员函数，
      *这个成员函数的参数是ThreadState类型的指针，返回类型是void。
    */
    void (Benchmark::*method)(ThreadState*) = nullptr;
    bool fresh_db = false;
    int num_threads = FLAGS_threads;

    // 设置benchmark方法
    ...

    // 如果需要刷新数据库，删除数据库重新打开
    if (fresh_db) {
      if (FLAGS_use_existing_db) {
        std::fprintf(stdout, "%-12s : skipped (--use_existing_db is true)\n",
                      name.ToString().c_str());
        method = nullptr;
      } else {
        delete db_;
        db_ = nullptr;
        DestroyDB(FLAGS_db, Options());
        Open();
      }
    }
    // 根据method 执行对应的Benchmark
    if (method != nullptr) {
      RunBenchmark(num_threads, name, method);
    }
  }
}
```

TODO: 分析每个Benchmark具体的执行逻辑

## 典型的Benchmark`WriteSeq()`

`WriteSeq()`只有一行代码，调用了`DoWrite(thread, true);`，让我们看看`DoWrite`做了什么

```cpp
void DoWrite(ThreadState* thread, bool seq) {
    // 如果不是预设的循环次数，提示该信息
    if (num_ != FLAGS_num) {
      char msg[100];
      std::snprintf(msg, sizeof(msg), "(%d ops)", num_);
      thread->stats.AddMessage(msg);
    }
    // 随机数据生成器
    RandomGenerator gen;
    WriteBatch batch;
    // TODO Status
    Status s;
    int64_t bytes = 0;
    // TODO KeyBuffer
    KeyBuffer key;
    for (int i = 0; i < num_; i += entries_per_batch_) {
      batch.Clear(); // 清空操作
      for (int j = 0; j < entries_per_batch_; j++) {
        // 根据传入的seq布尔值，选择进行随机写还是序列写
        const int k = seq ? i + j : thread->rand.Uniform(FLAGS_num);
        // 设置key
        key.Set(k);
        // 生成一个键的Slice,数据写入batch
        batch.Put(key.slice(), gen.Generate(value_size_));
        bytes += value_size_ + key.slice().size();
        // 单次循环结束
        thread->stats.FinishedSingleOp();
      }
      // 数据写入数据库，并获取写入结果
      s = db_->Write(write_options_, &batch);
      // 异常检查
      if (!s.ok()) {
        std::fprintf(stderr, "put error: %s\n", s.ToString().c_str());
        std::exit(1);
      }
    }
    // 设置状态
    thread->stats.AddBytes(bytes);
  }
```

整体逻辑还是比较简单的，接下来我们深入研究一下`Slice`，`WriteBatch`，`KeyBuffer`，`Status`都是什么

### Slice

代码来自`slice.h`

```cpp

//定义在namespace leveldb中
namespace leveldb {
//类的声明为`class LEVELDB_EXPORT Slice`：其中的`LEVELDB_EXPORT`为宏值，这种用法主要用于库符号的可见性控制，暂时不做分析
class LEVELDB_EXPORT Slice {
 public:
  // 创建一个空的Slice
  Slice() : data_(""), size_(0) {}

  // 创建一个Slice，这个Slice指向起始地址为d，长度为n字节的数据，也就是d[0,n-1]
  Slice(const char* d, size_t n) : data_(d), size_(n) {}

  // 创建一个Slice，这个Slice指向string类型的s的数据
  Slice(const std::string& s) : data_(s.data()), size_(s.size()) {}

  // 创建一个Slice，这个Slice指向起始地址为s，长度为strlen(s)字节的数据，也就是整个s字符串
  Slice(const char* s) : data_(s), size_(strlen(s)) {}

  // Intentionally copyable.
  // 设置拷贝构造函数为默认函数
  Slice(const Slice&) = default;
  // 设置拷贝赋值运算符为默认函数
  Slice& operator=(const Slice&) = default;

  // 返回指向数据起始位置的指针
  // const char* 返回值不能被修改
  // 第二个const代表该函数不会修改data_
  const char* data() const { return data_; }

  // 返回数据的长度
  size_t size() const { return size_; }

  // 判断Slice是否为空的数据
  bool empty() const { return size_ == 0; }

  // 返回第i字节的数据
  // 需要:n<size()
  char operator[](size_t n) const {
    assert(n < size());
    return data_[n];
  }

  // 清空Slice, 指向一个空字符串
  void clear() {
    data_ = "";
    size_ = 0;
  }

  // 移除Slice的前n个字符
  // 需要:n<size()
  void remove_prefix(size_t n) {
    assert(n <= size());
    data_ += n;
    size_ -= n;
  }

  // 返回数据的字符串形式
  std::string ToString() const { return std::string(data_, size_); }

  // 比较函数
  // Three-way comparison.  Returns value:
  //   <  0 iff "*this" <  "b",
  //   == 0 iff "*this" == "b",
  //   >  0 iff "*this" >  "b"
  int compare(const Slice& b) const;

  // 判断x是否是Slice的前缀
  bool starts_with(const Slice& x) const {
    return ((size_ >= x.size_) && (memcmp(data_, x.data_, x.size_) == 0));
  }

 private:
  const char* data_; //指向数据的指针
  size_t size_; //数据大小
};

// 重写 == 运算符
// 当且仅当size相同，数据相同时两个Slice的==运算成立
// 不需要x.data_ == y.data_
// 也就是不需要地址相同
inline bool operator==(const Slice& x, const Slice& y) {
  return ((x.size() == y.size()) &&
          (memcmp(x.data(), y.data(), x.size()) == 0));
}

// 重写 != 运算符
inline bool operator!=(const Slice& x, const Slice& y) { return !(x == y); }

// 实现compare
inline int Slice::compare(const Slice& b) const {
  // 取较短的长度
  const size_t min_len = (size_ < b.size_) ? size_ : b.size_;
  // 比较相同长度的数据比较结果
  // memcmp 小于返回负数 等于返回0 大于返回正数
  int r = memcmp(data_, b.data_, min_len);
  // 如果前min_len个字节相同
  if (r == 0) {
    // b比较长
    if (size_ < b.size_)
      r = -1;
    // 自己比较长
    else if (size_ > b.size_)
      r = +1;
  }
  return r;
}

}  // namespace leveldb

```

`Slice`的全部方法声明以及定义都位于`slice.h`文件中，而且也比较简单。总的来说，`Slice`主要是保存了一段数据的地址和大小，并且定义一些基础的构造函数和比较函数，可以视为对数据的一种封装

### WriteBatch

代码位于`write_batch.h`与`write_batch.cc`中
先看声明

```cpp
// 位于 leveldb命名空间中
namespace leveldb {

// 前置声明Slice
// ? 风格指南中提到要减少这种使用
// 优点：避免修改Slice导致一系列的重新编译
// 缺点：隐藏依赖关系等
class Slice;

class LEVELDB_EXPORT WriteBatch {
 public:
  //public 子类
  class LEVELDB_EXPORT Handler {
   public:
    virtual ~Handler();
    // 纯虚函数
    // 类似java的abstract方法
    // 写入某对数据
    virtual void Put(const Slice& key, const Slice& value) = 0;
    // 纯虚函数
    // 根据key删除数据
    virtual void Delete(const Slice& key) = 0;
  };
  //构造函数
  WriteBatch();

  // Intentionally copyable.
  // 设置拷贝构造函数为默认函数
  WriteBatch(const WriteBatch&) = default;
  // 设置拷贝赋值运算符为默认函数
  WriteBatch& operator=(const WriteBatch&) = default;
  // 析构函数
  ~WriteBatch();

  // 在数据库中存入key->value的映射
  void Put(const Slice& key, const Slice& value);

  // 清除数据库中key的键值对
  void Delete(const Slice& key);

  // 清除batch中所有已缓存的更新
  void Clear();

  // 返回数据库可能由于该batch操作导致的变化的大小
  // 这个数值与实现细节有关，可能会在不同的版本中有所变化。
  // 这个数值主要用于LevelDB的使用度量
  size_t ApproximateSize() const;

  // 从source中复制操作
  // 时间复杂度O(source.size())
  // 尽管这个方法的时间复杂度是线性的，但是常数因子比使用Handler在source上调用Iterate()方法要好。
  void Append(const WriteBatch& source);

  // Support for iterating over the contents of a batch.
  // 提供batch上的迭代方法
  Status Iterate(Handler* handler) const;

 private:
  // 提供一些不适合写到WriteBatch中的静态方法
  friend class WriteBatchInternal;
  // 详见write_batch.cc
  // 更新操作
  std::string rep_;  // See comment in write_batch.cc for the format of rep_
};

}  // namespace leveldb
```

看一下`WriteBatchInternal`的声明

```cpp
namespace leveldb {
//TODO
class MemTable;

class WriteBatchInternal {
 public:
  // 返回操作数
  static int Count(const WriteBatch* batch);

  // 设置操作数
  static void SetCount(WriteBatch* batch, int n);

  // 返回sequence number
  // typedef uint64_t SequenceNumber;
  static SequenceNumber Sequence(const WriteBatch* batch);

  // 设置sequence number
  static void SetSequence(WriteBatch* batch, SequenceNumber seq);

  // 直接返回rep_
  static Slice Contents(const WriteBatch* batch) { return Slice(batch->rep_); }

  static size_t ByteSize(const WriteBatch* batch) { return batch->rep_.size(); }
  // 设置rep_的内容
  static void SetContents(WriteBatch* batch, const Slice& contents);
  // 将rep_中的内容写入到数据库中
  static Status InsertInto(const WriteBatch* batch, MemTable* memtable);
  // 将src的内容添加到dst->rep_中
  static void Append(WriteBatch* dst, const WriteBatch* src);
};

}  // namespace leveldb

```

再看`write_batch.cc`

先看一下注释中给出的`rep_`的格式，这里的类型都是protobuf的数据格式：

```
WriteBatch::rep_ :=
   sequence: fixed64
   count: fixed32
   data: record[count]
record :=
   kTypeValue varstring varstring | kTypeDeletion varstring
varstring :=
   len: varint32
   data: uint8[len]
```
分析：

1. `rep_`包含三部分：`sequence`， `count`， `data`
2. `sequence`：一个固定64位的整数
3. `count`：一个固定32位的整数
4. `data`：一个record类型的数组，数组的长度由count字段指定
5. `record`：一个复合数据类型，它可以是以下两种形式之一
   1. kTypeValue varstring varstring 表示插入，更新等操作
   2. kTypeDeletion varstring 表示删除操作
6. `varstring`：一个复合数据类型，包含以下字段
   1. `len`：一个变长32位整数
   2. `data`：一个长度由len字段指定的uint8数组


接下来来看代码

```cpp

namespace leveldb {


// WriteBatch的字段rep_头部 sequence number + count 共12字节
static const size_t kHeader = 12;

// 构造函数做简单的清除操作
WriteBatch::WriteBatch() { Clear(); }
// 使用默认的析构函数
WriteBatch::~WriteBatch() = default;
// 使用默认的析构函数
WriteBatch::Handler::~Handler() = default;

// 清空rep_内容，重设rep_长度
void WriteBatch::Clear() {
  rep_.clear(); // 设置到0
  rep_.resize(kHeader); // 设置到12字节。但内容未定义
}

// 简单的返回了rep_的长度
// 可以理解为什么是Approximate
// 数据库中数据的存储格式是record格式
// 那边的record和这里的还不一样，有校验和等字段
// 这边还有sequence number， count
// 但是主体数据部分格式是相同的，都是data uint8[len]
// 所以是个大概的估计值
size_t WriteBatch::ApproximateSize() const { return rep_.size(); }


// 迭代方法
Status WriteBatch::Iterate(Handler* handler) const {
  // 包装rep_
  Slice input(rep_);
  // 长度检查
  if (input.size() < kHeader) {
    return Status::Corruption("malformed WriteBatch (too small)");
  }
  // 移除前两个字段
  input.remove_prefix(kHeader);
  // key和value
  Slice key, value;
  int found = 0;
  while (!input.empty()) {
    // 操作数+1
    found++;
    // 操作类型
    char tag = input[0];
    input.remove_prefix(1);
    switch (tag) {
      // enum leveldb::ValueType::kTypeValue = 1
      case kTypeValue:
        // GetLengthPrefixedSlice 取出固定长度的数据，长度来源于input中的len字段
        // 从input中拿出key和value
        if (GetLengthPrefixedSlice(&input, &key) &&
            GetLengthPrefixedSlice(&input, &value)) {
          // handler 中 放入键值对
          handler->Put(key, value);
        } else {
          return Status::Corruption("bad WriteBatch Put");
        }
        break;
      // enum leveldb::ValueType::kTypeDeletion = 0
      case kTypeDeletion:
        // 获取key
        if (GetLengthPrefixedSlice(&input, &key)) {
          // 删除对应值
          handler->Delete(key);
        } else {
          return Status::Corruption("bad WriteBatch Delete");
        }
        break;
      default:
        return Status::Corruption("unknown WriteBatch tag");
    }
  }
  // 如果操作数量对不上，报错
  if (found != WriteBatchInternal::Count(this)) {
    return Status::Corruption("WriteBatch has wrong count");
  } else {
    return Status::OK();
  }
}

// 取出count值
int WriteBatchInternal::Count(const WriteBatch* b) {
  return DecodeFixed32(b->rep_.data() + 8);
}

// 设置count值
void WriteBatchInternal::SetCount(WriteBatch* b, int n) {
  EncodeFixed32(&b->rep_[8], n);
}


// 取出SequenceNumber
SequenceNumber WriteBatchInternal::Sequence(const WriteBatch* b) {
  return SequenceNumber(DecodeFixed64(b->rep_.data()));
}


// 设置SequenceNumber
void WriteBatchInternal::SetSequence(WriteBatch* b, SequenceNumber seq) {
  EncodeFixed64(&b->rep_[0], seq);
}


// 向rep_中写入一个插入或更新等操作
void WriteBatch::Put(const Slice& key, const Slice& value) {
  // count++
  WriteBatchInternal::SetCount(this, WriteBatchInternal::Count(this) + 1);
  // 插入kTypeValue
  rep_.push_back(static_cast<char>(kTypeValue));
  // 插入key
  PutLengthPrefixedSlice(&rep_, key);
  // 插入value
  PutLengthPrefixedSlice(&rep_, value);
}

// 向rep_中写入一个删除操作
void WriteBatch::Delete(const Slice& key) {
  // count++
  WriteBatchInternal::SetCount(this, WriteBatchInternal::Count(this) + 1);
  // 插入kTypeDeletion
  rep_.push_back(static_cast<char>(kTypeDeletion));
  // 插入key
  PutLengthPrefixedSlice(&rep_, key);
}

// 委托WriteBatchInternal::Append执行该方法
void WriteBatch::Append(const WriteBatch& source) {
  WriteBatchInternal::Append(this, &source);
}

namespace {
// WriteBatch::Handler 的派生类
// 辅助WriteBatchInternal
class MemTableInserter : public WriteBatch::Handler {
 public:
  SequenceNumber sequence_;
  MemTable* mem_; // TODO

  void Put(const Slice& key, const Slice& value) override {
    mem_->Add(sequence_, kTypeValue, key, value); //TODO
    sequence_++;
  }
  void Delete(const Slice& key) override {
    mem_->Add(sequence_, kTypeDeletion, key, Slice()); //TODO
    sequence_++;
  }
};
}  // namespace

//向数据库中插入数据
//MemTableInserter 重写了Put和Delete方法
//从而正确的进行迭代
Status WriteBatchInternal::InsertInto(const WriteBatch* b, MemTable* memtable) {
  MemTableInserter inserter;
  inserter.sequence_ = WriteBatchInternal::Sequence(b);
  inserter.mem_ = memtable;
  return b->Iterate(&inserter);
}

void WriteBatchInternal::SetContents(WriteBatch* b, const Slice& contents) {
  assert(contents.size() >= kHeader);
  // 设置rep_的内容为contents的内容
  b->rep_.assign(contents.data(), contents.size());
}

//Append
void WriteBatchInternal::Append(WriteBatch* dst, const WriteBatch* src) {
  SetCount(dst, Count(dst) + Count(src)); // 更新操作树立
  assert(src->rep_.size() >= kHeader); //保证WriteBatch格式正确
  // src->rep_.data() + kHeader 忽略src的头部信息
  // src->rep_.size() - kHeader 写入这么多的字节
  dst->rep_.append(src->rep_.data() + kHeader, src->rep_.size() - kHeader);
}

}  // namespace leveldb

```

注意到下面的函数中使用了一个没有见过的类`MemTable`：

`Status WriteBatchInternal::InsertInto(const WriteBatch* b, MemTable* memtable)`

好在`MemTable`的声明较短，这里进行一个简单的分析，以下代码位于`memtable.h`，日后再在详细分析

```cpp
namespace leveldb {

//TODO
class InternalKeyComparator;
//TODO
class MemTableIterator;

class MemTable {
 public:
  // explicit 显式单参构造函数，避免隐式转换
  // MemTable引用计数
  // 初始引用为0。调用者必须至少调用一次Ref()
  explicit MemTable(const InternalKeyComparator& comparator);
  // 删除拷贝构造函数
  MemTable(const MemTable&) = delete;
  // 删除拷贝赋值运算符函数
  MemTable& operator=(const MemTable&) = delete;

  // 增加引用计数
  void Ref() { ++refs_; }

  // 减少引用计数，并在引用为0时销毁自身
  void Unref() {
    --refs_;
    assert(refs_ >= 0);
    if (refs_ <= 0) {
      delete this;
    }
  }


  // 此数据结构正在使用的数据字节数的估计值。并且，即使在修改MemTable时，调用此方法也是安全的。
  size_t ApproximateMemoryUsage();

  // 这个方法返回一个迭代器，这个迭代器可以用于访问memtable中的每个元素。
  // 调用者必须确保在使用返回的迭代器时，底层的MemTable仍然存在
  // 这个迭代器返回的键是由AppendInternalKey函数在db/format.{h,cc}模块中编码的内部键
  Iterator* NewIterator();


  // 将一个键值对添加到memtable中，同时指定一个序列号和类型
  // 如果类型是kTypeDeletion，那么值通常会是空的
  void Add(SequenceNumber seq, ValueType type, const Slice& key,
           const Slice& value);

  // 如果memtable中包含键对应的值，那么将这个值存储在*value中，并返回true。
  // 如果memtable中包含键对应的删除标记，那么将一个NotFound()错误存储在*status中，并返回true。
  // 如果memtable中既不包含键对应的值，也不包含键对应的删除标记，那么返回false
  bool Get(const LookupKey& key, std::string* value, Status* s);

 private:
  friend class MemTableIterator;
  friend class MemTableBackwardIterator;

  struct KeyComparator {
    const InternalKeyComparator comparator;
    explicit KeyComparator(const InternalKeyComparator& c) : comparator(c) {}
    int operator()(const char* a, const char* b) const;
  };

  typedef SkipList<const char*, KeyComparator> Table;

  ~MemTable();  // Private since only Unref() should be used to delete it

  KeyComparator comparator_;
  int refs_;
  Arena arena_;
  Table table_;
};

}  // namespace leveldb

```
`Memtable` 的实现暂时不分析，目前已经可以大致了解`WriteBatchInternal::InsertInto`在做什么了。事实上，`WriteBatchInternal::InsertInto`将逐个读取保存在`WriteBatch`中的所有操作记录，并利用`MemTableInserter`真正的保存到数据库中

### Status

### KeyBuffer