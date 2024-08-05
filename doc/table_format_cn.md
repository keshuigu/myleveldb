# leveldb 文件格式

```
<beginning_of_file>
[data block 1]
[data block 2]
...
[data block N]
[meta block 1]
...
[meta block K]
[metaindex block]
[index block]
[Footer]        (固定大小; 起始位置：file_size - sizeof(Footer))
<end_of_file>
```

该文件包含内部指针。每个这样的指针都被称为BlockHandle，并包含以下信息：

```
offset:   varint64
size:     varint64
```

> [varints](https://developers.google.com/protocol-buffers/docs/encoding#varints)
> varint64将64位整型压缩到1-10个字节

1. 文件中的键/值对序列按排序顺序存储，并划分为数据块序列。这些块一个接一个地出现在文件的开头。每个数据块都根据`block_builder.cc`”`中的代码进行格式化，然后可选地进行压缩。
2. 在数据块之后，存储了一堆元块。支持的元块类型如下所述。未来可能会添加更多的元块类型。每个元块再次使用`block_builder.cc`进行格式化，然后可选地进行压缩。
3. 一个`metaindex block`块。它包含每个其他元块的一个条目，其中键是元块的名称，值是指向该元块的BlockHandle。
4. 一个`index`块。此块包含每个数据块的一个条目，其中键是>=该数据块中的最后一个键的字符串，并且小于位于连续数据块中第一个键；值是数据块的BlockHandle。
5. 在文件的最后是一个固定长度的页脚，其中包含`metaindex`和`index`块的BlockHandle以及一个幻数。

```
metaindex_handle: char[p];     // Block handle for metaindex
index_handle:     char[q];     // Block handle for index
padding:          char[40-p-q];// zeroed bytes to make fixed length
                               // (40==2*BlockHandle::kMaxEncodedLength)
magic:            fixed64;     // == 0xdb4775248b80fb57 (little-endian)
```

## `filter`元块

如果在打开数据库时指定了`FilterPolicy`，则会在每个表中存储一个筛选器块。`metaindex`块包含一个从`filter映射的条目<N>`到筛选器块的BlockHandle，其中`<N>`是筛选器策略的`Name()`方法返回的字符串。

筛选器块存储一系列筛选器，其中筛选器`i`存储了文件偏移量在范围`[ i*base ... (i+1)*base-1 ]`内`FilterPolicy::CreateFilter()`输出的所有键,

目前，`base`为2KB。因此，如果块X和Y开始在范围`[0KB..2KB-1]`内，则通过调用`FilterPolicy:：CreateFilter()`，X和Y中的所有键都将转换为筛选器，并且生成的筛选器将存储为筛选器块中的第一个筛选器。

筛选器块格式如下：

```
[filter 0]
[filter 1]
[filter 2]
...
[filter N-1]

[offset of filter 0]                  : 4 bytes
[offset of filter 1]                  : 4 bytes
[offset of filter 2]                  : 4 bytes
...
[offset of filter N-1]                : 4 bytes

[offset of beginning of offset array] : 4 bytes
lg(base)                              : 1 byte
```

筛选器块末端的`offset array`指向从偏移到对应筛选器的映射关系的起始偏移。

## `stats`元块

这个元块包含一堆统计数据：键是统计信息的名称，值包含统计信息。
```
data size
index size
key size (uncompressed)
value size (uncompressed)
number of entries
number of data blocks
```