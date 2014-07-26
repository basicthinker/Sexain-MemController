// addr_trans_table.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "addr_trans_table.h"

using namespace std;

const char* ATTEntry::state_strings[] = {
    "CLEAN", "LOAN", "FREE", "DIRTY", "HIDDEN", "TEMP", "STAINED"
};

int AddrTransTable::Lookup(Tag phy_tag, Profiler& pf) {
  pf.AddTableOp();
  unordered_map<Tag, int>::iterator it = tag_index_.find(phy_tag);
  if (it == tag_index_.end()) { // not hit
    return -EINVAL;
  } else {
    ATTEntry& entry = entries_[it->second];
    assert(entry.state != ATTEntry::FREE && entry.phy_tag == phy_tag);
    // LRU
    GetQueue(entry.state).Remove(it->second);
    GetQueue(entry.state).PushBack(it->second);
    return it->second;
  }
}

int AddrTransTable::Setup(Tag phy_tag, Addr mach_base, ATTEntry::State state,
    Profiler& pf) {
  assert(tag_index_.count(phy_tag) == 0);
  assert(!GetQueue(ATTEntry::FREE).Empty());

  int i = GetQueue(ATTEntry::FREE).PopFront();
  GetQueue(state).PushBack(i);
  entries_[i].state = state;
  entries_[i].phy_tag = phy_tag;
  entries_[i].mach_base = mach_base;

  tag_index_[phy_tag] = i;
  pf.AddTableOp();
  return i;
}

void AddrTransTable::ShiftState(int index, ATTEntry::State new_state,
    Profiler& pf) {
  ATTEntry& entry = entries_[index];
  assert(entry.state != new_state);
  if (new_state == ATTEntry::FREE) {
    tag_index_.erase(entry.phy_tag);
  }
  GetQueue(entry.state).Remove(index);
  GetQueue(new_state).PushBack(index);
  entries_[index].state = new_state;
  pf.AddTableOp();
}

void AddrTransTable::Reset(int index,
    Addr new_base, ATTEntry::State new_state, Profiler& pf) {
  ATTEntry& entry = entries_[index];
  entry.mach_base = new_base;
  ShiftState(index, new_state, pf);
}

void AddrTransTable::ClearStats(Profiler& pf) {
  for (vector<ATTEntry>::iterator it = entries_.begin(); it != entries_.end();
      ++it) {
    it->epoch_reads = 0;
    it->epoch_writes = 0;
  }
  pf.AddTableOp(); // assumed in parallel
}
