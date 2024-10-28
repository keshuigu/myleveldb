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
```

4. `GTEST_AMBIGUOUS_ELSE_BLOCKER_`多层嵌套情况下，if与else配对可能有歧义 套一个switch去除