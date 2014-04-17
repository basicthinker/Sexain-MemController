// addr_trans_table.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "addr_trans_table.h"

using namespace std;

uint64_t AddrTransTable::Lookup(uint64_t phy_tag, EntryState* state) {
  unordered_map<uint64_t, int>::iterator it = tag_index_.find(phy_tag);
  if (it == tag_index_.end()) { // not hit
    if (state) *state = FREE_ENTRY;
    return Addr(phy_tag);
  } else {
    ATTEntry& entry = entries_[it->second];
    assert(entry.state != FREE_ENTRY && entry.phy_tag == phy_tag);
    queues_[entry.state].Remove(it->second);
    queues_[entry.state].PushBack(it->second);
    if (state) *state = entry.state;
    return entry.mach_addr;
  }
}

void AddrTransTable::Setup(uint64_t phy_tag, uint64_t mach_addr,
    uint32_t flag) {
  assert(tag_index_.count(phy_tag) == 0);
  assert(!queues_[FREE_ENTRY].Empty());

  int i = queues_[FREE_ENTRY].PopFront();
  queues_[DIRTY_ENTRY].PushBack(i);
  entries_[i].state = DIRTY_ENTRY;
  entries_[i].phy_tag = phy_tag;
  entries_[i].mach_addr = mach_addr;
  entries_[i].flag = flag;

  tag_index_[phy_tag] = i;
}

void AddrTransTable::RevokeEntry(int index) {
  ATTEntry& entry = entries_[index];
  assert(entry.state != FREE_ENTRY);
  tag_index_.erase(entry.phy_tag); 
  queues_[entry.state].Remove(index);

  entry = ATTEntry();
  queues_[FREE_ENTRY].PushBack(index);
}

void AddrTransTable::Revoke(uint64_t phy_tag) {
  unordered_map<uint64_t, int>::iterator it = tag_index_.find(phy_tag);
  if (it != tag_index_.end()) {
    assert(entries_[it->second].phy_tag == phy_tag);
    RevokeEntry(it->second);
  }
}

pair<uint64_t, uint64_t> AddrTransTable::Replace(
    uint64_t phy_tag, uint64_t mach_addr) {
  const int i = queues_[CLEAN_ENTRY].Front();
  assert(i != -EINVAL);
  pair<uint64_t, uint64_t> replaced;
  replaced.first = entries_[i].phy_tag;
  replaced.second = entries_[i].mach_addr;
  Revoke(entries_[i].phy_tag);
  Setup(phy_tag, mach_addr);
  return replaced;
}

int AddrTransTable::CleanDirtied() {
  int i = queues_[DIRTY_ENTRY].PopFront();
  int count = 0;
  while (i != -EINVAL) {
    assert(entries_[i].state == DIRTY_ENTRY);
    entries_[i].state = CLEAN_ENTRY;
    queues_[CLEAN_ENTRY].PushBack(i);
    ++count;
    i = queues_[DIRTY_ENTRY].PopFront();
  }
  assert(queues_[DIRTY_ENTRY].Empty() &&
      GetLength(CLEAN_ENTRY) + GetLength(FREE_ENTRY) == length_);
  return count;
}

int AddrTransTable::RemoveFlagged(uint32_t flag) {
  assert(flag != 0);
  int count = 0;
  for (int i = 0; i < length_; ++i) {
    if (entries_[i].flag == flag) {
      RevokeEntry(i); // cannot be free entry
      ++count;
    }
  }
  return count;
}

