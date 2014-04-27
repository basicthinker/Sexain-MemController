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
    assert(entry.state != ATTEntry::FREE && entry.phy_tag == phy_tag);
    queues_[entry.state].Remove(it->second);
    queues_[entry.state].PushBack(it->second);
    if (index) *index = it->second;
    return entry.mach_base;
  }
}

void AddrTransTable::Setup(uint64_t phy_tag, uint64_t mach_base,
    ATTEntry::State state, uint32_t flag) {
  assert(tag_index_.count(phy_tag) == 0);
  assert(!queues_[ATTEntry::FREE].Empty());

  int i = queues_[ATTEntry::FREE].PopFront();
  queues_[state].PushBack(i);
  entries_[i].state = state;
  entries_[i].phy_tag = phy_tag;
  entries_[i].mach_base = mach_base;
  assert((flag != ATTEntry::NON_TEMP) == (state == ATTEntry::TEMP));
  entries_[i].flag = flag;

  tag_index_[phy_tag] = i;
}

void AddrTransTable::FreeEntry(int index) {
  ATTEntry& entry = entries_[index];
  assert(entry.state != ATTEntry::FREE);
  tag_index_.erase(entry.phy_tag); 
  queues_[entry.state].Remove(index);
  queues_[ATTEntry::FREE].PushBack(index);
}

void AddrTransTable::Revoke(uint64_t phy_tag) {
  unordered_map<uint64_t, int>::iterator it = tag_index_.find(phy_tag);
  if (it != tag_index_.end()) {
    assert(entries_[it->second].phy_tag == phy_tag);
    FreeEntry(it->second);
  }
}

pair<uint64_t, uint64_t> AddrTransTable::Replace(uint64_t phy_tag,
    uint64_t mach_base, ATTEntry::State state, uint32_t flag) {
  const int i = queues_[ATTEntry::CLEAN].Front();
  assert(i != -EINVAL);
  assert(entries_[i].flag == 0);
  pair<uint64_t, uint64_t> replaced;
  replaced.first = entries_[i].phy_tag;
  replaced.second = entries_[i].mach_base;
  FreeEntry(i);
  Setup(phy_tag, mach_base, state, flag);
  return replaced;
}

void AddrTransTable::Reset(int index, uint64_t mach_base,
    ATTEntry::State state, uint32_t flag_mask) {
  ATTEntry& entry = entries_[index];
  entry.mach_base = mach_base;
  queues_[entry.state].Remove(index);
  queues_[state].PushBack(index);
  entry.state = state;
  assert(entry.flag != ATTEntry::NON_TEMP);
  entry.Set(ATTEntry::MACH_RESET | flag_mask);
}

int AddrTransTable::CleanDirtyQueue() {
  int count = 0;
  int i = queues_[ATTEntry::DIRTY].PopFront();
  while (i != -EINVAL) {
    assert(entries_[i].state == ATTEntry::DIRTY);
    entries_[i].state = ATTEntry::CLEAN;
    queues_[ATTEntry::CLEAN].PushBack(i);
    ++count;
    i = queues_[ATTEntry::DIRTY].PopFront();
  }
  assert(queues_[ATTEntry::DIRTY].Empty());
  return count;
}

int AddrTransTable::FreeQueue(ATTEntry::State state) {
  int count = 0;
  int i = queues_[state].Front();
  while (i != -EINVAL) {
    FreeEntry(i);
    ++count;
    i = queues_[state].Front();
  }
  return count;
}

