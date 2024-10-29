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