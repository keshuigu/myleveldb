#ifndef STORAGE_LEVELDB_INCLUDE_FILTER_POLICY_H_
#define STORAGE_LEVELDB_INCLUDE_FILTER_POLICY_H_
// 数据库可以配置一个自定义的 FilterPolicy 对象。
// 这个对象负责从一组键中创建一个小的过滤器。
// 这些过滤器存储在 leveldb 中，并由 leveldb
// 自动查询，以决定是否从磁盘读取某些信息。 在许多情况下，过滤器可以将在每次
// DB::Get() 调用的磁盘查找次数从几次减少到一次。
//
// 大多数人会希望使用内置的布隆过滤器支持（见下文的 NewBloomFilterPolicy()）。

#include <string>

#include "leveldb/export.h"

namespace leveldb {
class Slice;
class LEVELDB_EXPORT FilterPolicy {
 public:
  virtual ~FilterPolicy();

  // 返回此策略的名称。请注意，如果过滤器编码以不兼容的方式更改，
  // 则此方法返回的名称必须更改。否则，旧的不兼容过滤器可能会传递给此类型。
  virtual const char* Name() const = 0;

  // keys[0,n-1]
  // 包含一个键列表（可能包含重复项），这些键根据用户提供的比较器排序 将 总结
  // keys[0,n-1] 的过滤器附加到 *dst。
  //
  // 警告：不会更改 *dst 的初始内容。相反，会将新构建的过滤器附加到 *dst。
  virtual void CreateFilter(const Slice* keys, int n,
                            std::string* dst) const = 0;

  // "filter" 包含之前对该类的 CreateFilter() 调用附加的数据。
  // 如果键在传递给 CreateFilter() 的键列表中，则此方法必须返回 true。
  // 如果键不在列表中，此方法可以返回 true 或 false，但应尽量以较高概率返回
  // false
  virtual bool KeyMayMatch(const Slice& key, const Slice& filter) const = 0;
};

// 返回一个新的过滤策略，该策略使用布隆过滤器，每个键大约使用指定数量的位。
// 对于 bits_per_key，一个好的值是 10，这会产生一个约 1% 误报率的过滤器。
//
// 调用者必须在关闭使用该结果的任何数据库后删除该结果。
//
// 注意：如果您使用的自定义比较器忽略了被比较键的某些部分，则不能使用
// NewBloomFilterPolicy()， 必须提供您自己的FilterPolicy，也忽略键的相应部分。
// 例如，如果比较器忽略尾随空格，则使用不忽略键中尾随空格的
// FilterPolicy（如NewBloomFilterPolicy）是不正确的。
LEVELDB_EXPORT const FilterPolicy* NewBloomFilterPolicy(int bits_per_key);
}  // namespace leveldb

#endif