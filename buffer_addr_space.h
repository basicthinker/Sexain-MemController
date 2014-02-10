// buffer_addr_space.h

#ifndef SEXAIN_BUFFER_ADDR_SPACE_H_
#define SEXAIN_BUFFER_ADDR_SPACE__H_

#include <vector>

class BufferAddrSpace {
 public:
  virtual uint64_t AllocBufferAddr(uint64_t phy_addr, int att_index) = 0;
  virtual void FreeBufferAddr(uint64_t mach_addr) = 0;
};

class DirectMappingBuffer : public BufferAddrSpace {
 public:
  DirectMappingBuffer(int att_size, int phy_limit) : att_size_(att_size),
      phy_limit_(phy_limit), alloc_map_(att_size, false) { }

  uint64_t phy_limit() const { return phy_limit_; }

  uint64_t AllocBufferAddr(uint64_t phy_addr, int att_index) {
    assert(att_index < att_size_ && alloc_map_[att_index] == false);
    alloc_map_[att_index] = true; 
    return att_index + phy_limit_;
  }

  void FreeBufferAddr(uint64_t mach_addr) {
    assert(alloc_map_[mach_addr - phy_limit_]);
    alloc_map_[mach_addr - phy_limit_] = false;
  }

 private:
  const int att_size_;
  const uint64_t phy_limit_;
  std::vector<bool> alloc_map_;
};

#endif // SEXAIN_BUFFER_ADDR_SPACE_H_
