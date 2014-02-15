// shadow_tag_mapper.h
// Copyright (c) 2014 Jinglei Ren <jinglei.ren@stanzax.org>

#ifndef SEXAIN_SHADOW_TAG_MAPPER_H_
#define SEXAIN_SHADOW_TAG_MAPPER__H_

#include <vector>
#include <cstdint>

class ShadowTagMapper {
 public:
  virtual uint64_t MapShadowTag(uint64_t phy_tag, int att_index) = 0;
  virtual void UnmapShadowTag(uint64_t mach_tag) = 0;
};

class DirectMapper : public ShadowTagMapper {
 public:
  DirectMapper(int att_length, int num_phy_tags) : att_length_(att_length),
      num_phy_tags_(num_phy_tags), alloc_map_(att_length, false) { }

  uint64_t num_phy_tags() const { return num_phy_tags_; }

  uint64_t MapShadowTag(uint64_t phy_tag, int att_index) {
    assert(att_index < att_length_ && alloc_map_[att_index] == false);
    alloc_map_[att_index] = true; 
    return att_index + num_phy_tags_;
  }

  void UnmapShadowTag(uint64_t mach_tag) {
    assert(alloc_map_[mach_tag - num_phy_tags_]);
    alloc_map_[mach_tag - num_phy_tags_] = false;
  }

 private:
  const int att_length_;
  const uint64_t num_phy_tags_;
  std::vector<bool> alloc_map_;
};

#endif // SEXAIN_SHADOW_TAG_MAPPER_H_
