// shadow_tag_mapper.h
// Copyright (c) 2014 Jinglei Ren <jinglei.ren@stanzax.org>

#ifndef SEXAIN_SHADOW_TAG_MAPPER_H_
#define SEXAIN_SHADOW_TAG_MAPPER_H_

#include <vector>
#include <cstdint>

class ShadowTagMapper {
 public:
  virtual uint64_t MapShadowTag(uint64_t phy_tag, int table_index) = 0;
  virtual void UnmapShadowTag(uint64_t mach_tag) = 0;
  virtual int length() const = 0;
  virtual int floor() const = 0;
  virtual void set_floor(int tag) = 0;
};

class DirectMapper : public ShadowTagMapper {
 public:
  DirectMapper(int length) : length_(length),
      alloc_map_(length, false) {
    set_floor(-EINVAL);
  }

  uint64_t MapShadowTag(uint64_t phy_tag, int table_index) {
    assert(floor() >= 0);
    assert(table_index < length() && alloc_map_[table_index] == false);
    alloc_map_[table_index] = true; 
    return floor() + table_index;
  }

  void UnmapShadowTag(uint64_t mach_tag) {
    assert(floor() >= 0);
    assert(alloc_map_[mach_tag - floor()]);
    alloc_map_[mach_tag - floor()] = false;
  }

  int length() const { return length_; }
  int floor() const { return floor_; }
  void set_floor(int tag) { floor_ = tag; }

 private:
  const int length_;
  int floor_;
  std::vector<bool> alloc_map_;
};

#endif // SEXAIN_SHADOW_TAG_MAPPER_H_

