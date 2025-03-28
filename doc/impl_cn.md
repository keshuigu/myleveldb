## 文件

leveldb的实现在思想上类似于单个[Bigtable tablet (section 5.3)](https://research.google/pubs/pub27898/)。但是，组成表示层的文件结构有些不同，下面将对此进行解释。

每个数据库都由存储在目录中的一组文件表示。有几种不同类型的文件，如下所示：

### 日志文件

日志文件`(*.log)`存储一系列最近的更新。每次更新都会附加到当前日志文件。当日志文件达到预定大小时(默认情况下约为4MB)，它被转换为排序表(见下文)并且为将来的更新创建新的日志文件。当前日志文件的副本保存在内存结构中`(memtable)`。每次读取时都会查阅此副本，以便读取操作反映所有记录的更新。

## 有序表

有序表`(*.ldb)`存储按关键字排序的条目序列。每个条目都是键对应的值，或者是键的删除标记。(删除标记保留在周围，以隐藏旧排序表中存在的过时值)。

有序表集合被组织成一系列级别。从日志文件生成的排序表被放置在一个特殊的年轻级别(也称为`level-0`)。当年轻文件的数量超过某个阈值(目前为四个)时，所有年轻文件都会与所有重叠的一级文件合并在一起，以生成一系列新的一级文档(我们为每2MB的数据创建一个新的一层文档)。

年轻级别的文件可能包含重叠的键。但是，其他级别的文件具有不同的、不重叠的关键字范围。考虑层数`L`，其中`L>=1`。当`level-L`的文件的组合大小超过`(10^L)MB`(即，10MB用于1级，100MB用于2级，…)时，`level-L`中的一个文件和所有重叠的`level-(L+1)`文件被合并，以形成一组新的`level-(L+1)`的文件。这些合并的效果是仅使用批量读取和写入将新更新从年轻级别逐渐迁移到最大级别(即，最大限度地减少昂贵的查找)。

### MANIFEST

`MANIFEST`文件列出了组成每个级别的一组有序表、相应的键范围和其他重要元数据。每当数据库重新打开时，都会创建一个新的`MANIFEST`文件(文件名中嵌入一个新数字)。`MANIFEST`文件被格式化为日志，并且对服务状态所做的更改(添加或删除文件时)被附加到该日志中。

### Current

`CURRENT`是一个简单的文本文件，包含最新`MANIFEST`文件的名称。

### Info logs

信息消息将打印到名为`LOG`和`LOG.old`的文件中

### 其他

其他用途的其他文件(`LOCK`、`*.dbtmp`)。

## level 0

当日志文件增长到一定大小(默认为4MB)以上时：创建一个全新的`memtable`和日志文件，用新文件记录未来的更新。

在后台：

1. 将上一个`memtable`的内容写入`sstable`。
2. 丢弃`memtable`的内容。
3. 删除旧的日志文件和旧的`memtable`。
4. 将新的`sstable`添加到年轻(`level-0`)级别

## 压缩

当级别L的大小超过其限制时，我们在后台线程中压缩它。压缩从`level-L`中拾取一个文件，并从下一级别`level-(L+1)`中拾取所有重叠的文件。请注意，如果一个`level-L`文件只与一个`level-(L+1)`级文件的一部分重叠，则位于`level-(L+1)`级的整个文件将被用作压缩的输入，并在压缩后被丢弃。此外：由于`level-0`是特殊的(其中的文件可能相互重叠)，我们特别处理从`level-0`到1级的压缩：`level-0`压缩可能会选择多个`level-0`文件，以防其中一些文件相互重叠。

压缩合并所选文件的内容以生成一系列`level-(L+1)`文件。在当前输出文件达到目标文件大小（2MB）后，我们继续生成一个新的`level-(L+1)`文件。当当前输出文件的键范围增长到多于十个`level-(L+2)`文件重叠时，我们也会切换到新的输出文件。最后一条规则确保以后对`level-(L+1)`文件的压缩不会从`level-(L+2)`中拾取太多数据。

旧文件将被丢弃，并将新文件添加到服务状态。

特定级别的压缩会在键空间中轮换。更详细地说，对于每个级别L，我们会保存`level-L`上一次压缩的结束键。`level-L`的下一次压缩将选择在该键之后开始的第一个文件（如果没有这样的文件，则轮换到键空间的开头）。

压缩会丢弃被覆盖的值。如果没有包含范围与当前关键字重叠的更高级别的文件，当前文件中的删除标记也会被丢弃。

### Timing

`level-0`压缩将从`level-0`读取最多四个1MB的文件，最坏情况下读取所有`level-1`文件（10MB）。即，我们将读取14MB并写入14MB。

**这一段有问题**
除了特殊的`level-0`压缩之外，我们将从`level-L`中选择一个2MB的文件。在最坏的情况下，这将与`level-(L+1)`的约12个文件重叠（10个文件，因为`level-(L+1)`是`level-L`的十倍大，另外两个文件在边界处，因为`level-L`的文件范围通常不会与`level-(L+1)`的文件范围对齐）。因此，压缩将读取26MB并写入26MB。假设磁盘IO速率为100MB/s（现代驱动器的大致范围），最差的压缩成本约为0.5秒。

如果我们将后台写入限制在较小的速度，例如100MB/s全速的10%，则压缩可能需要5秒。如果用户以10MB/s的速度写入，我们可能会构建大量的`level-0`文件（大约50个文件来容纳这5*10MB）。由于每次读取时将更多文件合并在一起的开销，这可能会显著增加读取成本。

解决方案1：为了减少这个问题，当`level-0`文件的数量很大时，我们可能需要增加日志切换阈值。虽然缺点是这个阈值越大，我们需要容纳相应memtable的内存就越多

解决方案2：当`level-0`文件的数量增加时，我们可能希望人为地降低写入速率。


解决方案3：致力于降低非常广泛的合并成本。也许大多数`level-0`文件的块都未压缩在缓存中，我们只需要担心合并迭代器中的O(N)复杂性。

### 文件数量

我们可以为更大的级别制作更大的文件，以减少文件总数，而不是总是制作2MB的文件，尽管这是以更突发的压缩为代价的。或者，我们可以将文件集分割到多个目录中。

2011年2月4日，在ext3文件系统上进行的一项实验显示了在文件数量不同的目录中打开100K文件所使用的时间：

| Files in directory | Microseconds to open a file |
| -----------------: | --------------------------: |
|               1000 |                           9 |
|              10000 |                          10 |
|             100000 |                          16 |

所以，也许在现代文件系统上甚至不需要进行分片？

## 恢复

- 读取CURRENT以查找最近提交的MANIFEST的名称
- 读取命名的MANIFEST文件
- 清理过时的文件
- 我们可以在这里打开所有的表，但懒惰执行可能更好...
- 将日志区块转换为新的`level-0 sstable`
- 开始将新写入定向到具有恢复序列的新日志文件#

## Garbage collection of files

`RemoveObsoleteFiles()`在每次压缩结束和恢复结束时调用。它查找数据库中所有文件的名称。它会删除所有不是当前日志文件的日志文件。它删除所有未从某个级别引用，同时也不是活动压缩的输出的表文件。