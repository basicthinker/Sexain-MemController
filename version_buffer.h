// version_buffer.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_VERSION_BUFFER_H_
#define SEXAIN_VERSION_BUFFER_H_

#include <vector>
#include <set>
#include <limits>
#include <cstdint>
#include <cassert>

#define INVAL_ADDR std::numeric_limits<uint64_t>::max()

class VersionBuffer {
 public:
  enum State {
    IN_USE = 0,
    BACKUP,
    FREE,
  };

  VersionBuffer(int length, int block_bits);

  uint64_t NewBlock();
  void FreeBlock(uint64_t mach_addr, State bs);
  void PinBlock(uint64_t mach_addr);
  void FreeBackup();

  uint64_t addr_base() const { return addr_base_; }
  void set_addr_base(uint64_t base) { addr_base_ = base; }
  int length() const { return length_; }
  int block_size() const { return 1 << block_bits_; }
  ///
  /// The total address space size that this buffer area covers in bytes
  ///
  uint64_t Size() const;
  bool Contains(uint64_t addr) const;
 private:
  uint64_t At(int index);
  int Index(uint64_t mach_addr);

  uint64_t addr_base_;
  const int length_;
  const int block_bits_;
  const uint64_t block_mask_;
  std::vector<std::set<int>> sets_;
};

inline VersionBuffer::VersionBuffer(int length, int block_bits) :
    length_(length), block_bits_(block_bits),
    block_mask_(block_size() - 1), sets_(3) {
  for (int i = 0; i < length_; ++i) {
    sets_[FREE].insert(i);
  }
  addr_base_ = INVAL_ADDR;
}

inline uint64_t VersionBuffer::Size() const {
  return length_ << block_bits_;
}

inline bool VersionBuffer::Contains(uint64_t addr) const {
  return addr >= addr_base_ && (addr - addr_base_) < Size();
}

inline uint64_t VersionBuffer::At(int index) {
  assert(addr_base_ != INVAL_ADDR && index >= 0 && index < length_);
  return addr_base_ + (index << block_bits_);
}

inline int VersionBuffer::Index(uint64_t mach_addr) {
  assert(mach_addr >= addr_base_);
  uint64_t bytes = mach_addr - addr_base_;
  assert((bytes & block_mask_) == 0);
  int i = bytes >> block_bits_;
  assert(i >= 0 && i < length_);
  return i;
}

#endif // SEXAIN_VERSION_BUFFER_H_

