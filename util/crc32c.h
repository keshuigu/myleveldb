#ifndef STORAGE_LEVELDB_UTIL_CRC32C_H_
#define STORAGE_LEVELDB_UTIL_CRC32C_H_

#include <cstddef>
#include <cstdint>

namespace leveldb {
namespace crc32c {

// 返回 concat(A, data[0,n-1]) 的 crc32c，其中 init_crc 是某个字符串 A 的
// crc32c。 Extend() 通常用于维护数据流的 crc32c。
uint32_t Extend(uint32_t init_crc, const char* data, size_t n);

// 返回 data[0,n-1] 的 crc32c
inline uint32_t Value(const char* data, size_t n) { return Extend(0, data, n); }

static const uint32_t kMaskDelta = 0xa282ead8ul;

// 返回 crc 的掩码表示。
//
// 动机：计算包含嵌入 CRC 的字符串的 CRC 是有问题的。
// 因此，我们建议在存储 CRC（例如，在文件中）之前对其进行掩码处理。
inline uint32_t Mask(uint32_t crc) {
  return ((crc >> 15) | (crc << 17)) + kMaskDelta;
}

inline uint32_t Unmask(uint32_t masked_crc) {
  masked_crc -= kMaskDelta;
  return ((masked_crc >> 17) | (masked_crc << 15));
}

}  // namespace crc32c

}  // namespace leveldb

#endif