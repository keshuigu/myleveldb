#include "util/arena.h"

namespace leveldb {
static const int kBlockSize = 4096;

Arena::Arena()
    : alloc_ptr_(nullptr), alloc_bytes_remaining_(0), memory_usage_(0) {}

Arena::~Arena() {
  for (size_t i = 0; i < blocks_.size(); i++) {
    delete[] blocks_[i];
  }
}

char* Arena::AllocateFallback(size_t bytes){
  // 进入该函数的前提是剩余空间不足以分配
  if (bytes > kBlockSize /4)
  {
    // 如果需要分配的大小大于块的四分之一
    // 单独存放
    char* result = AllocateNewBlock(bytes);
    return result;
  }

  // 抛弃当前块的剩余空间
  alloc_ptr_ = AllocateNewBlock(kBlockSize);
  alloc_bytes_remaining_ = kBlockSize;
  char* result = alloc_ptr_;
  alloc_ptr_ += bytes;
  alloc_bytes_remaining_ -= bytes;
  return result;
}

char* Arena::AllocateAligned(size_t bytes){
  const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
  // align 必须是2的幂
  static_assert((align & (align - 1)) == 0,"Pointer size should be a power of 2");
  // 超出上一个对其位置的字节数
  size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align - 1);
  size_t slop = (current_mod == 0) ? 0 : align - current_mod;
  // 从当前位置开始需要分配的字节数
  size_t needed = bytes + slop;
  char* result;
  if (needed <= alloc_bytes_remaining_)
  {
    result = alloc_ptr_ + slop;
    alloc_ptr_ += needed;
    alloc_bytes_remaining_ -= needed;
  } else{
    // AllocateFallback always returned aligned memory
    // 块大小为4k,必定对齐
    result = AllocateFallback(bytes);
  }
  // 对齐
  assert((reinterpret_cast<uintptr_t>(result) & (align - 1)) == 0);
  return result;
}

char* Arena::AllocateNewBlock(size_t block_bytes) {
  char* result = new char[block_bytes];
  blocks_.push_back(result);
  memory_usage_.fetch_add(block_bytes + sizeof(char*), std::memory_order_relaxed);
  return result;
}

}  // namespace leveldb
