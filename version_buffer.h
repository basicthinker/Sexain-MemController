// version_buffer.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_VERSION_BUFFER_H_
#define SEXAIN_VERSION_BUFFER_H_

#include <vector>
#include <set>
#include <cstdint>
#include <cassert>

enum BufferState {
  IN_USE_SLOT = 0,
  BACKUP_SLOT,
  FREE_SLOT,
};

class VersionBuffer {
 public:
  VersionBuffer(int length, int block_bits);
  ~VersionBuffer();

  uint8_t* NewBlock();
  void FreeBlock(uint8_t* mach_addr, BufferState bs);
  void PinBlock(uint8_t* mach_addr);

  int block_size() const { return 1 << block_bits_; }
  int length() const { return length_; }
 private:
  uint8_t* At(int index);
  int Index(uint8_t* mach_addr);

  uint8_t* data;
  const int block_bits_;
  const int block_mask_;
  const int length_;
  std::vector<std::set<int>> sets_;
};

inline VersionBuffer::VersionBuffer(int length, int block_bits) :
    length_(length), block_bits_(block_bits),
    block_mask_(block_size() - 1), sets_(3) {
  data = new uint8_t[length_ << block_bits_];
  for (int i = 0; i < length_; ++i) {
    sets_[FREE_SLOT].insert(i);
  }
}

inline VersionBuffer::~VersionBuffer() {
  delete[] data;
}

inline uint8_t* VersionBuffer::At(int index) {
  assert(index >= 0 && index < length_);
  return data + (index << block_bits_);
}

inline int VersionBuffer::Index(uint8_t* mach_addr) {
  int bytes = mach_addr - data;
  assert(bytes >= 0 && (bytes & block_mask_) == 0);
  int i = bytes >> block_bits_;
  assert(i < length_);
  return i;
}

#endif // SEXAIN_VERSION_BUFFER_H_

