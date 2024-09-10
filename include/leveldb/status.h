#ifndef STORAGE_LEVELDB_INCLUDE_STATUS_H_
#define STORAGE_LEVELDB_INCLUDE_STATUS_H_

#include <algorithm>
#include <string>

#include "leveldb/export.h"
#include "leveldb/slice.h"

namespace leveldb {
class Status {
 public:
  // Create ok status
  Status() noexcept : state_(nullptr) {};
  ~Status() {
    delete[] state_;
  }  // delete nullptr 指针是有定义的行为。简单类型下，delete[] 同 delete

  Status(const Status& rhs);
  Status& operator=(const Status& rhs);

  Status(Status&& rhs) noexcept : state_(rhs.state_) {
    rhs.state_ = nullptr;
  }  // 移动构造
  Status& operator=(Status&& rhs) noexcept;  // 移动赋值构造

  // Return Ok Status
  static Status OK() { return Status(); }

  static Status NotFound(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kNotFound, msg, msg2);
  }

  static Status Corruption(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kCorruption, msg, msg2);
  }

  static Status NotSupported(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kNotSupported, msg, msg2);
  }

  static Status InvalidArgument(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kInvalidArgument, msg, msg2);
  }
  static Status IOError(const Slice& msg, const Slice& msg2 = Slice()) {
    return Status(kIOError, msg, msg2);
  }

  // 检查状态

  bool ok() const { return (state_ == nullptr); }
  bool IsNotFound() const { return code() == kNotFound; }
  bool IsCorruption() const { return code() == kCorruption; }
  bool IsNotSupported() const { return code() == kNotSupported; }
  bool IsInvalidArgument() const { return code() == kInvalidArgument; }
  bool IsIOError() const { return code() == kIOError; }

  std::string ToString() const;

 private:
  enum Code {
    kOk = 0,
    kNotFound = 1,
    kCorruption = 2,
    kNotSupported = 3,
    kInvalidArgument = 4,
    kIOError = 5,
  };

  Code code() const {
    return (state_ == nullptr) ? kOk : static_cast<Code>(state_[4]);
  }

  Status(Code code, const Slice& msg, const Slice& msg2);
  static const char* CopyState(const char* s);

  // 状态信息
  // kOK -> null
  // 其他状态
  // state_[0..3] == len of message
  // state_[4] == code
  // state_[5..n) == message
  const char* state_;
};

inline Status::Status(const Status& rhs){
  state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
}

inline Status& Status::operator=(const Status& rhs){
  if (state_!=rhs.state_)
  {
    delete[] state_; // 移除现有状态
    state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
  }
  // 如果是同一个status或者都是ok状态，无需操作
  return *this;
}

inline Status& Status::operator=(Status && rhs){
  std::swap(state_,rhs.state_); // 使用move后，原本的值应该假设为不再使用，因此不需要清除原本的值
  return *this;
}

}  // namespace leveldb

#endif // STORAGE_LEVELDB_INCLUDE_STATUS_H_