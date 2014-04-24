// addr_trans_table.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "addr_trans_table.h"

using namespace std;

uint64_t AddrTransTable::Lookup(uint64_t phy_tag, int* index) {
  unordered_map<uint64_t, int>::iterator it = tag_index_.find(phy_tag);
  if (it == tag_index_.end()) { // not hit
    if (index) *index = -EINVAL;
    return Addr(phy_tag);
  } else {
    ATTEntry& entry = entries_[it->second];
    assert(entry.state != FREE_ENTRY && entry.phy_tag == phy_tag);
    queues_[entry.state].Remove(it->second);
    queues_[entry.state].PushBack(it->second);
    if (index) *index = it->second;
    return entry.mach_base;
  }
}

void AddrTransTable::Setup(uint64_t phy_tag, uint64_t mach_base,
    EntryState state, uint32_t flag_mask) {
  assert(tag_index_.count(phy_tag) == 0);
  assert(!queues_[FREE_ENTRY].Empty());

  int i = queues_[FREE_ENTRY].PopFront();
  queues_[state].PushBack(i);
  entries_[i].state = state;
  entries_[i].phy_tag = phy_tag;
  entries_[i].mach_base = mach_base;
  entries_[i].Set(flag_mask);

  tag_index_[phy_tag] = i;
}

void AddrTransTable::FreeEntry(int index) {
  ATTEntry& entry = entries_[index];
  assert(entry.state != FREE_ENTRY);
  tag_index_.erase(entry.phy_tag); 
  queues_[entry.state].Remove(index);
  queues_[FREE_ENTRY].PushBack(index);
}

void AddrTransTable::Revoke(uint64_t phy_tag) {
  unordered_map<uint64_t, int>::iterator it = tag_index_.find(phy_tag);
  if (it != tag_index_.end()) {
    assert(entries_[it->second].phy_tag == phy_tag);
    FreeEntry(it->second);
  }
}

pair<uint64_t, uint64_t> AddrTransTable::Replace(uint64_t phy_tag,
    uint64_t mach_base, EntryState state, uint32_t flag_mask) {
  const int i = queues_[CLEAN_ENTRY].Front();
  assert(i != -EINVAL);
  assert(entries_[i].flag == 0);
  pair<uint64_t, uint64_t> replaced;
  replaced.first = entries_[i].phy_tag;
  replaced.second = entries_[i].mach_base;
  Revoke(entries_[i].phy_tag);
  Setup(phy_tag, mach_base, state, flag_mask);
  return replaced;
}

void AddrTransTable::Reset(int index, uint64_t mach_base,
    EntryState state, uint32_t flag_mask) {
  ATTEntry& entry = entries_[index];
  entry.mach_base = mach_base;
  queues_[entry.state].Remove(index);
  queues_[state].PushBack(index);
  entry.state = state;
  entry.Set(ATTEntry::MACH_RESET | flag_mask);
}

int AddrTransTable::CleanDirtyQueue() {
  int count = 0;
  int i = queues_[DIRTY_ENTRY].PopFront();
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

int AddrTransTable::FreeQueue(EntryState state) {
  int count = 0;
  int i = queues_[state].Front();
  while (i != -EINVAL) {
    FreeEntry(i);
    ++count;
    i = queues_[state].Front();
  }
  return count;
}

