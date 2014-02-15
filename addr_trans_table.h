// addr_trans_table.h
// Copyright (c) 2014 Jinglei Ren <jinglei.ren@stanzax.org>

#ifndef SEXAIN_ADDR_TRANS_TABLE_H_
#define SEXAIN_ADDR_TRANS_TABLE_H_

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

class AddrTransTable : public IndexArray {
 public:
  AddrTransTable(int length, int bits, ShadowTagMapper& mapper, MemStore& mem);
  uint64_t LoadAddr(uint64_t phy_addr);
  uint64_t StoreAddr(uint64_t phy_addr);
  int block_size() const { return 1 << block_bits_; }
  IndexNode& operator[](int i) { return entries_[i].queue_node; }
  void NewEpoch();
 private:
  uint64_t Tag(uint64_t addr) { return addr >> block_bits_; }
  uint64_t Trans(uint64_t orig_addr, uint64_t new_tag) {
    return (orig_addr & block_mask_) + (new_tag << block_bits_);
  }

  const int block_bits_;
  const uint64_t block_mask_;
  ShadowTagMapper& mapper_;
  MemStore& mem_store_;
  std::unordered_map<uint64_t, int> trans_index_;
  std::vector<ATTEntry> entries_;
  std::vector<IndexQueue> queues_;
};

AddrTransTable::AddrTransTable(int length, int bits,
    ShadowTagMapper& mapper, MemStore& mem) :
    block_bits_(bits), block_mask_((1 << bits) - 1),
    mapper_(mapper), mem_store_(mem), entries_(length), queues_(2, *this) {
  for (int i = 0; i < length; ++i) {
    queues_[0].PushBack(i); // clean queue
  }
}

uint64_t AddrTransTable::LoadAddr(uint64_t phy_addr) {
  std::unordered_map<uint64_t, int>::iterator it =
      trans_index_.find(Tag(phy_addr));
  if (it == trans_index_.end()) { // not hit
    return phy_addr;
  } else {
    ATTEntry& entry = entries_[it->second];
    assert(entry.valid && entry.phy_tag == Tag(phy_addr));
    queues_[entry.dirty].Remove(it->second);
    queues_[entry.dirty].PushBack(it->second);
    return Trans(phy_addr, entry.mach_tag);
  }
}

uint64_t AddrTransTable::StoreAddr(uint64_t phy_addr) {
  uint64_t phy_tag = Tag(phy_addr);
  std::unordered_map<uint64_t, int>::iterator it = trans_index_.find(phy_tag);

  if (it == trans_index_.end()) { // not hit
    if (queues_[0].Empty()) NewEpoch();
    int i = queues_[0].PopFront(); // clean queue
    ATTEntry& entry = entries_[i];

    if (entry.valid) {
      mem_store_.OnWriteBack(entry.phy_tag, entry.mach_tag);
      trans_index_.erase(entry.phy_tag);
      mapper_.UnmapShadowTag(entry.mach_tag);
    }

    uint64_t mach_tag = mapper_.MapShadowTag(phy_addr, i);
    if (!entry.valid) mem_store_.OnDirectWrite(phy_tag, mach_tag);

    entries_[i].phy_tag = phy_tag;
    entries_[i].mach_tag = mach_tag;
    entries_[i].dirty = true;
    entries_[i].valid = true;

    queues_[1].PushBack(i); // dirty queue
    trans_index_[phy_tag] = i;
    return Trans(phy_addr, mach_tag);

  } else { // hit
    ATTEntry& entry = entries_[it->second];
    assert(entry.valid && entry.phy_tag == phy_tag);
    queues_[entry.dirty].Remove(it->second);
    if (entry.dirty) {
      mem_store_.OnOverwrite(entry.phy_tag, entry.mach_tag);
      queues_[1].PushBack(it->second);
      return Trans(phy_addr, entry.mach_tag);
    } else {
      mem_store_.OnShrink(entry.phy_tag, entry.mach_tag);
      trans_index_.erase(it);
      entry.valid = false;
      queues_[0].PushFront(it->second);
      mapper_.UnmapShadowTag(entry.mach_tag);
      return phy_addr;
    }
  }
}

void AddrTransTable::NewEpoch() {
  mem_store_.OnEpochEnd();
  assert(IndexIntersection(queues_[0], queues_[1]).size() == 0);
  while (!queues_[1].Empty()) {
    int i = queues_[1].PopFront();
    entries_[i].dirty = false;
    queues_[0].PushBack(i);
  }
  assert(queues_[0].Indexes().size() == entries_.size());
}

#endif // SEXAIN_ADDR_TRANS_TABLE_H_

