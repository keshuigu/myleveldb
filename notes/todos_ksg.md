# 学习cpp
1. read doc
2. make comment on code
3. try it


# 边边角角
1. `if(object)` 执行的是对象的重载的`operator bool()`
2. GOOGLE TEST 的 `ASSERT_*` 可以用 `<<` 接着输出
3. 补充2

```cpp
ASSERT_EQ(cases[i].type, type) << f;
// 展开后
switch (0)
case 0:
default:
  if (const ::testing::AssertionResult gtest_ar =
          (::testing::internal::EqHelper::Compare("cases[i].type", "type",
                                                  cases[i].type, type)))
    ;
  else
    return ::testing::internal::AssertHelper(
                ::testing::TestPartResult::kFatalFailure,
                "/home/yl/workspace/myleveldb/db/filename_test.cc", 34,
                gtest_ar.failure_message()) = ::testing::Message() << f;
// The GNU compiler emits a warning if nested "if" statements are followed by
// an "else" statement and braces are not used to explicitly disambiguate the
// "else" binding.  This leads to problems with code like:
//
//   if (gate)
//     ASSERT_*(condition) << "Some message";
//
// The "switch (0) case 0:" idiom is used to suppress this.
```

4. `GTEST_AMBIGUOUS_ELSE_BLOCKER_`多层嵌套情况下，if与else配对可能有歧义 套一个switch去除
5. `alignof(decltype(instance_storage_))>= alignof(InstanceType)`, `decltype` 是一个 C++11 引入的关键字，用于获取表达式的类型, `alignof` 是一个 C++11 引入的运算符，用于获取类型的对齐要求（以字节为单位）
6. placement new: 语法是 `new (address) Type(arguments)`，其中 `address` 是预先分配的内存地址，`Type` 是要构造的对象类型，`arguments` 是传递给构造函数的参数

```cpp
new (&instance_storage_)
        InstanceType(std::forward<ConstructorArgTypes>(constructor_args)...);
```

7. `std::forward` 是一个 C++11 引入的标准库函数模板，用于实现完美转发。它可以将参数完美地转发给另一个函数，保留参数的左值或右值属性。
8. `using` 可以从头文件传递，慎用，如`iterator.h`中的`using CleanupFunction = void (*)(void* arg1, void* arg2);`

# 布隆过滤器

布隆过滤器（Bloom Filter）是一种空间效率高的概率型数据结构，用于测试一个元素是否属于一个集合。它可以快速地判断某个元素是否不在集合中，但可能会误判某个元素在集合中（即存在一定的误报率）。
工作原理
布隆过滤器由一个位数组和一组哈希函数组成。其基本操作包括插入和查询：

插入：

对要插入的元素，使用一组哈希函数计算出多个哈希值。
根据这些哈希值，将位数组中对应位置的位设置为1。

查询：

对要查询的元素，使用同样的一组哈希函数计算出多个哈希值。
检查位数组中对应位置的位是否都为1。
如果所有位都为1，则元素可能在集合中（存在误报的可能）。
如果有任何一位为0，则元素一定不在集合中。

优点：

1. 空间效率高：布隆过滤器使用的空间远小于存储所有元素的集合。
2. 插入和查询操作速度快：时间复杂度为O(k)，其中k是哈希函数的数量。

缺点：

1. 存在误报率：布隆过滤器可能会误判某个元素在集合中，但不会漏报。
2. 不支持删除操作：一旦插入元素，无法删除，因为删除可能会影响其他元素的判断。