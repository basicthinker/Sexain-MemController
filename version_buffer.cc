// version_buffer.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "version_buffer.h"

using namespace std;

uint64_t VersionBuffer::NewBlock() {
  assert(!sets_[FREE].empty());
  int i = *sets_[FREE].begin();
  sets_[FREE].erase(sets_[FREE].begin());
  sets_[IN_USE].insert(i);
  return At(i);
}

void VersionBuffer::FreeBlock(uint64_t mach_addr, State state) {
  int i = Index(mach_addr);
  set<int>::iterator it = sets_[state].find(i);
  assert(it != sets_[state].end());
  sets_[state].erase(it);
  sets_[FREE].insert(i);
}

void VersionBuffer::BackupBlock(uint64_t mach_addr, State state) {
  int i = Index(mach_addr);
  assert(state == BACKUP0 || state == BACKUP1);
  assert(sets_[state].count(i) == 0);
  set<int>::iterator it = sets_[IN_USE].find(i);
  assert(it != sets_[IN_USE].end());
  sets_[IN_USE].erase(it);
  sets_[state].insert(i);
}

int VersionBuffer::ClearBackup() {
  int num = 0;
  for(set<int>::iterator it = sets_[BACKUP0].begin();
      it != sets_[BACKUP0].end(); ++it) {
    sets_[FREE].insert(*it);
    ++num;
  }
  sets_[BACKUP0].clear();

  for(set<int>::iterator it = sets_[BACKUP1].begin();
      it != sets_[BACKUP1].end(); ++it) {
    sets_[BACKUP0].insert(*it);
    ++num;
  }
  sets_[BACKUP1].clear();

  assert(sets_[IN_USE].size() + sets_[FREE].size() +
      sets_[BACKUP0].size() == (unsigned int)length_);
  return num;
}

