// addr_trans_table.cc
// Copyright (c) 2014 Jinglei Ren <jinglei.ren@stanzax.org>

#include "addr_trans_table.h"

uint64_t AddrTransTable::LoadAddr(uint64_t phy_addr) {
  uint64_t phy_tag = Tag(phy_addr);
  std::unordered_map<uint64_t, int>::iterator it = tag_index_.find(phy_tag);
  if (it == tag_index_.end()) { // not hit
    return phy_addr;
  } else {
    ATTEntry& entry = entries_[it->second];
    assert(entry.valid && entry.phy_tag == phy_tag);
    queues_[entry.dirty].Remove(it->second);
    queues_[entry.dirty].PushBack(it->second);
    return Trans(phy_addr, entry.mach_tag);
  }
}

uint64_t AddrTransTable::StoreAddr(uint64_t phy_addr) {
  uint64_t phy_tag = Tag(phy_addr);
  std::unordered_map<uint64_t, int>::iterator it = tag_index_.find(phy_tag);
  if (it == tag_index_.end()) { // not hit
    assert(!queues_[0].Empty());
    int i = queues_[0].PopFront(); // clean queue
    ATTEntry& entry = entries_[i];

    if (entry.valid) {
      mem_store_.OnWriteBack(entry.phy_tag, entry.mach_tag, block_bits_);
      tag_index_.erase(entry.phy_tag);
      mapper_.UnmapShadowTag(entry.mach_tag);
    }

    uint64_t mach_tag = mapper_.MapShadowTag(phy_tag, i);
    if (!entry.valid) mem_store_.OnDirectWrite(phy_tag, mach_tag, block_bits_);

    entries_[i].phy_tag = phy_tag;
    entries_[i].mach_tag = mach_tag;
    entries_[i].dirty = true;
    entries_[i].valid = true;

    queues_[1].PushBack(i); // dirty queue
    tag_index_[phy_tag] = i;
    return Trans(phy_addr, mach_tag);

  } else { // hit
    ATTEntry& entry = entries_[it->second];
    assert(entry.valid && entry.phy_tag == phy_tag);
    queues_[entry.dirty].Remove(it->second);
    if (entry.dirty) {
      mem_store_.OnOverwrite(entry.phy_tag, entry.mach_tag, block_bits_);
      queues_[1].PushBack(it->second);
      return Trans(phy_addr, entry.mach_tag);
    } else {
      mem_store_.OnShrink(entry.phy_tag, entry.mach_tag, block_bits_);
      tag_index_.erase(it);
      entry.valid = false;
      queues_[0].PushFront(it->second);
      mapper_.UnmapShadowTag(entry.mach_tag);
      return phy_addr;
    }
  }
}

