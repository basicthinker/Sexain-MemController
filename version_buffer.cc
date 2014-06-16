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

void VersionBuffer::FreeBlock(uint64_t mach_addr, State bs) {
  int i = Index(mach_addr);
  set<int>::iterator it = sets_[bs].find(i);
  assert(it != sets_[bs].end());
  sets_[bs].erase(it);
  sets_[FREE].insert(i);
}

void VersionBuffer::PinBlock(uint64_t mach_addr) {
  int i = Index(mach_addr);
  assert(sets_[FREE].count(i) == 0);
  set<int>::iterator it = sets_[IN_USE].find(i);
  assert(it != sets_[IN_USE].end());
  sets_[IN_USE].erase(it);
  sets_[BACKUP].insert(i);
}

void VersionBuffer::FreeBackup() {
  for(set<int>::iterator it = sets_[BACKUP].begin();
      it != sets_[BACKUP].end(); ++it) {
    sets_[FREE].insert(*it);
  }
  sets_[BACKUP].clear();
  assert(sets_[IN_USE].size() + sets_[FREE].size() == (unsigned int)length_);
}

