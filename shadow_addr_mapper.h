// shadow_addr_mapper.h
// Copyright (c) 2014 Jinglei Ren <jinglei.ren@stanzax.org>

#ifndef SEXAIN_SHADOW_ADDR_MAPPER_H_
#define SEXAIN_SHADOW_ADDR_MAPPER__H_

#include <vector>
#include <cstdint>

class ShadowAddrMapper {
 public:
  virtual uint64_t MapShadowAddr(uint64_t phy_addr, int att_index) = 0;
  virtual void UnmapShadowAddr(uint64_t mach_addr) = 0;
};

class DirectMapper : public ShadowAddrMapper {
 public:
  DirectMapper(int att_length, int phy_limit) : att_length_(att_length),
      phy_limit_(phy_limit), alloc_map_(att_length, false) { }

  uint64_t phy_limit() const { return phy_limit_; }

  uint64_t MapShadowAddr(uint64_t phy_addr, int att_index) {
    assert(att_index < att_length_ && alloc_map_[att_index] == false);
    alloc_map_[att_index] = true; 
    return att_index + phy_limit_;
  }

  void UnmapShadowAddr(uint64_t mach_addr) {
    assert(alloc_map_[mach_addr - phy_limit_]);
    alloc_map_[mach_addr - phy_limit_] = false;
  }

 private:
  const int att_length_;
  const uint64_t phy_limit_;
  std::vector<bool> alloc_map_;
};

#endif // SEXAIN_SHADOW_ADDR_MAPPER_H_
