// addr_trans_table.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "addr_trans_table.h"

uint8_t* AddrTransTable::Lookup(uint64_t phy_addr) {
  uint64_t phy_tag = Tag(phy_addr);
  std::unordered_map<uint64_t, int>::iterator it = tag_index_.find(phy_tag);
  if (it == tag_index_.end()) { // not hit
    return NULL;
  } else {
    ATTEntry& entry = entries_[it->second];
    assert(entry.state != FREE_ENTRY && entry.phy_tag == phy_tag);
    queues_[entry.state].Remove(it->second);
    queues_[entry.state].PushBack(it->second);
    return Trans(phy_addr, entry.mach_addr);
  }
}

void AddrTransTable::SetupTag(uint64_t phy_tag, uint8_t* mach_addr) {
  assert(tag_index_.find(phy_tag) == tag_index_.end());
  assert(!queues_[FREE_ENTRY].Empty());
  int i = queues_[FREE_ENTRY].PopFront();
  tag_index_[phy_tag] = i;
  queues_[DIRTY_ENTRY].PushBack(i);

  entries_[i].phy_tag = phy_tag;
  entries_[i].mach_addr = mach_addr;
  entries_[i].state = DIRTY_ENTRY;
}

int AddrTransTable::Clean() {
  int i = queues_[DIRTY_ENTRY].PopFront();
  while (i != -EINVAL) {
    queues_[CLEAN_ENTRY].PushBack(i);
    assert(entries_[i].state == DIRTY_ENTRY);
    entries_[i].state = CLEAN_ENTRY;
    i = queues_[DIRTY_ENTRY].PopFront();
  }
  assert(queues_[DIRTY_ENTRY].Empty() &&
      GetLength(CLEAN_ENTRY) + GetLength(FREE_ENTRY) == length_);
}

void AddrTransTable::RevokeTag(uint64_t tag) {
  std::unordered_map<uint64_t, int>::iterator it = tag_index_.find(tag);
  if (it != tag_index_.end()) {
    ATTEntry& entry = entries_[it->second];
    assert(entry.state != FREE_ENTRY && entry.phy_tag == tag);
    queues_[entry.state].Remove(it->second);
    queues_[FREE_ENTRY].PushBack(it->second);
    entry.state = FREE_ENTRY;
    tag_index_.erase(it);
  }
}

