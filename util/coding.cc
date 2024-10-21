#include "util/coding.h"

#include "leveldb/slice.h"

namespace leveldb {
void PutFixed32(std::string* dst, uint32_t value) {
  char buf[sizeof(value)];
  EncodeFixed32(buf, value);
  dst->append(buf, sizeof(buf));
}
void PutFixed64(std::string* dst, uint64_t value) {
  char buf[sizeof(value)];
  EncodeFixed64(buf, value);
  dst->append(buf, sizeof(buf));
}

char* EncodeVarint32(char* dst, uint32_t value) {
  static const int B = 128;  // 1000_0000
  uint8_t* ptr = reinterpret_cast<uint8_t*>(dst);
  while (value >= B) {
    *(ptr++) = value | B;
    value >>= 7;
  }
  *(ptr++) = static_cast<uint8_t>(value);
  return reinterpret_cast<char*>(ptr);
}

char* EncodeVarint64(char* dst, uint64_t value) {
  uint8_t* ptr = reinterpret_cast<uint8_t*>(dst);
  static const int B = 128;  // 1000_0000
  while (value >= B) {
    *(ptr++) = value | B;
    value >>= 7;
  }
  *(ptr++) = static_cast<uint8_t>(value);
  return reinterpret_cast<char*>(ptr);
}

void PutVarint32(std::string* dst, uint32_t value) {
  char buf[5];  // 32 / 7 = 4 .. 4
  char* ptr = EncodeVarint32(buf, value);
  dst->append(buf, ptr - buf);
}

void PutVarint64(std::string* dst, uint64_t value) {
  char buf[10];  // 64 / 7 = 9 .. 1
  char* ptr = EncodeVarint64(buf, value);
  dst->append(buf, ptr - buf);
}

void PutLengthPrefixedSlice(std::string* dst, const Slice& value) {
  // len + data
  PutVarint32(dst, value.size());
  dst->append(value.data(), value.size());
}

int VarintLength(uint64_t v) {
  int len = 1;
  static const int B = 128;  // 1000_0000
  while (v >= B) {
    len++;
    v >>= 7;
  }
  return len;
}

bool GetVarint32(Slice* input, uint32_t* value) {
  const char* p = input->data();
  const char* limit = p + input->size();
  const char* q = GetVarint32Ptr(p, limit, value);
  if (q == nullptr) {
    return false;
  } else {
    *input = Slice(q, limit - q);
    return true;
  }
}

bool GetVarint64(Slice* input, uint64_t* value) {
  const char* p = input->data();
  const char* limit = p + input->size();
  const char* q = GetVarint64Ptr(p, limit, value);
  if (q == nullptr) {
    return false;
  } else {
    *input = Slice(q, limit - q);
    return true;
  }
}

bool GetLengthPrefixedSlice(Slice* input, Slice* result) {
  uint32_t len;
  if (GetVarint32(input, &len) && input->size() >= len) { // input已经修改
    *result = Slice(input->data(), len);
    input->remove_prefix(len);
    return true;
  }
  return false;
}

const char* GetVarint64Ptr(const char* p, const char* limit, uint64_t* v) {
  uint64_t result = 0;
  for (uint32_t shift = 0; shift <= 63 && p < limit; shift += 7) {
    uint64_t byte = *(reinterpret_cast<const uint8_t*>(p));
    p++;
    if (byte & 128) {                     // msb == 1
      result |= ((byte & 127) << shift);  // 小端序
    } else {
      result |= (byte << shift);
      *v = result;
      return reinterpret_cast<const char*>(p);
    }
  }
  return nullptr;
}

const char* GetVarint32PtrFallback(const char* p, const char* limit,
                                   uint32_t* value) {
  uint32_t result = 0;
  for (uint32_t shift = 0; shift <= 28 && p < limit; shift += 7) {
    uint32_t byte = *(reinterpret_cast<const uint8_t*>(p));
    p++;
    if (byte & 128) {                     // msb == 1
      result |= ((byte & 127) << shift);  // 小端序
    } else {
      result |= (byte << shift);
      *value = result;
      return reinterpret_cast<const char*>(p);
    }
  }
  return nullptr;
}
}  // namespace leveldb
