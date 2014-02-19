// shadow_tag_mapper.h
// Copyright (c) 2014 Jinglei Ren <jinglei.ren@stanzax.org>

#ifndef SEXAIN_SHADOW_TAG_MAPPER_H_
#define SEXAIN_SHADOW_TAG_MAPPER_H_

#include <vector>
#include <cstdint>

class ShadowTagMapper {
 public:
  virtual uint64_t MapShadowTag(uint64_t phy_tag, int tmt_index) = 0;
  virtual void UnmapShadowTag(uint64_t mach_tag) = 0;
  virtual int lower_limit() const = 0;
  virtual void set_lower_limit(int tags) = 0;
  virtual int upper_limit() const = 0;
};

class DirectMapper : public ShadowTagMapper {
 public:
  DirectMapper(int tmt_length) : tmt_length_(tmt_length),
      alloc_map_(tmt_length, false) {
    set_lower_limit(-EINVAL);
  }

  uint64_t MapShadowTag(uint64_t phy_tag, int tmt_index) {
    assert(lower_limit() >= 0);
    assert(tmt_index < tmt_length_ && alloc_map_[tmt_index] == false);
    alloc_map_[tmt_index] = true; 
    return tmt_index + lower_limit();
  }

  void UnmapShadowTag(uint64_t mach_tag) {
    assert(lower_limit() >= 0);
    assert(alloc_map_[mach_tag - lower_limit()]);
    alloc_map_[mach_tag - lower_limit()] = false;
  }

  int lower_limit() const { return lower_limit_; }
  void set_lower_limit(int tags) { lower_limit_ = tags; }
  int upper_limit() const { return lower_limit() + tmt_length_; }

 private:
  const int tmt_length_;
  int lower_limit_;
  std::vector<bool> alloc_map_;
};

#endif // SEXAIN_SHADOW_TAG_MAPPER_H_

