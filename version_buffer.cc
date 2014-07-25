// version_buffer.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "version_buffer.h"

using namespace std;

uint64_t VersionBuffer::SlotAlloc(Profiler& pf) {
  assert(!sets_[FREE].empty());
  int i = *sets_[FREE].begin();
  sets_[FREE].erase(sets_[FREE].begin());
  sets_[IN_USE].insert(i);
  pf.AddBufferOp();
  return At(i);
}

void VersionBuffer::FreeSlot(uint64_t mach_addr, State state, Profiler& pf) {
  int i = Index(mach_addr);
  set<int>::iterator it = sets_[state].find(i);
  assert(it != sets_[state].end());
  sets_[state].erase(it);
  sets_[FREE].insert(i);
  pf.AddBufferOp();
}

void VersionBuffer::SlotBackup(uint64_t mach_addr, State state, Profiler& pf) {
  int i = Index(mach_addr);
  assert(state == BACKUP0 || state == BACKUP1);
  assert(sets_[state].count(i) == 0);
  set<int>::iterator it = sets_[IN_USE].find(i);
  assert(it != sets_[IN_USE].end());
  sets_[IN_USE].erase(it);
  sets_[state].insert(i);
  pf.AddBufferOp();
}

void VersionBuffer::ClearBackup(Profiler& pf) {
  for(set<int>::iterator it = sets_[BACKUP0].begin();
      it != sets_[BACKUP0].end(); ++it) {
    sets_[FREE].insert(*it);
  }
  sets_[BACKUP0].clear();
  pf.AddBufferOp(); // assumed in parallel

  for(set<int>::iterator it = sets_[BACKUP1].begin();
      it != sets_[BACKUP1].end(); ++it) {
    sets_[BACKUP0].insert(*it);
  }
  sets_[BACKUP1].clear();
  pf.AddBufferOp(); // assumed in parallel

  assert(sets_[IN_USE].size() + sets_[FREE].size() +
      sets_[BACKUP0].size() == (unsigned int)length_);
}

