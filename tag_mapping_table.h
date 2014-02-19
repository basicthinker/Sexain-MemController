// tag_mapping_table.h
// Copyright (c) 2014 Jinglei Ren <jinglei.ren@stanzax.org>

#ifndef SEXAIN_TAG_MAPPING_TABLE_H_
#define SEXAIN_TAG_MAPPING_TABLE_H_

#include <cerrno>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <iostream>
#include "index_queue.h"
#include "shadow_tag_mapper.h"
#include "mem_store.h"

struct ATTEntry {
  uint64_t phy_tag;
  uint64_t mach_tag;
  IndexNode queue_node;
  bool dirty;
  bool valid;

  ATTEntry() : dirty(false), valid(false) {
    queue_node.first = queue_node.second = -EINVAL;
  }
};

class TagMappingTable : public IndexArray {
 public:
  TagMappingTable(int length, int bits, ShadowTagMapper& mapper, MemStore& mem);

  bool Probe(uint64_t phy_tag);
  uint64_t LoadTag(uint64_t phy_tag);
  uint64_t StoreTag(uint64_t phy_tag);
  void NewEpoch();

  int block_size() const { return 1 << block_bits_; }
  int length() const { return entries_.size(); }
  IndexNode& operator[](int i) { return entries_[i].queue_node; }

 private:
  const int block_bits_;
  const uint64_t block_mask_;
  ShadowTagMapper& mapper_;
  MemStore& mem_store_;
  std::unordered_map<uint64_t, int> tag_index_;
  std::vector<ATTEntry> entries_;
  std::vector<IndexQueue> queues_;
};

inline TagMappingTable::TagMappingTable(int length, int bits,
    ShadowTagMapper& mapper, MemStore& mem) :
    block_bits_(bits), block_mask_((1 << bits) - 1),
    mapper_(mapper), mem_store_(mem), entries_(length), queues_(2, *this) {
  for (int i = 0; i < length; ++i) {
    queues_[0].PushBack(i); // clean queue
  }
}

inline bool TagMappingTable::Probe(uint64_t phy_tag) {
  return !queues_[0].Empty() || tag_index_.find(phy_tag) != tag_index_.end();
}

inline void TagMappingTable::NewEpoch() {
  mem_store_.OnEpochEnd(block_bits_);
  assert(IndexIntersection(queues_[0], queues_[1]).size() == 0);
  while (!queues_[1].Empty()) {
    int i = queues_[1].PopFront();
    entries_[i].dirty = false;
    queues_[0].PushBack(i);
  }
  assert(queues_[0].GetLength() == entries_.size());
}

#endif // SEXAIN_TAG_MAPPING_TABLE_H_

