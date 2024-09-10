#include "leveldb/status.h"

#include <cstdio>
#include <cstring>

#include "port/port.h"

namespace leveldb {
const char* Status::CopyState(const char* state) {
  uint32_t size;
  std::memcpy(&size, state, sizeof(size));  // 复制长度字段内容
  char* result = new char[size + 5];
  std::memcpy(result, state, size + 5);
  return result;
}

Status::Status(Code code, const Slice& msg, const Slice& msg2) {
  assert(code != kOk);
  const uint32_t len1 = static_cast<uint32_t>(msg.size());
  const uint32_t len2 = static_cast<uint32_t>(msg2.size());
  const uint32_t size = len1 + (len2 ? (2 + len2) : 0);  // 2字节": "
  char* result = new char[size + 5];
  std::memcpy(result, &size, sizeof(size));  // 4字节长度
  result[4] = static_cast<char>(code);       // 第5字节为状态码
  std::memcpy(result + 5, msg.data(), len1);
  if (len2) {
    result[5 + len1] = ':';
    result[6 + len1] = ' ';
    std::memcpy(result + 7 + len1, msg2.data(), len2);
  }
  state_ = result;
}

std::string Status::ToString() const {
  if (state_ == nullptr) {
    return "OK";
  } else {
    char tmp[30];
    const char* type;
    switch (code()) {
      case kOk:
        type = "OK";
        break;
      case kNotFound:
        type = "NotFound: ";
        break;
      case kCorruption:
        type = "Corruption: ";
        break;
      case kNotSupported:
        type = "NotSupported: ";
        break;
      case kInvalidArgument:
        type = "InvalidArgument: ";
        break;
      case kIOError:
        type = "IOError: ";
        break;
      default:
        std::snprintf(tmp,sizeof(tmp),"Unknown code(%d)",static_cast<int>(code()));
        type = tmp;
        break;
    }
    std::string result(type);
    uint32_t length;
    std::memcpy(&length,state_,sizeof(length));
    result.append(state_+5,length);
    return result;
  }
}

}  // namespace leveldb
