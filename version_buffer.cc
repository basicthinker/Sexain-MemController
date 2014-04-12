// version_buffer.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "version_buffer.h"

uint8_t* VersionBuffer::NewBlock() {
  assert(!free_.empty());
  int i = *free_.begin();
  free_.erase(free_.begin());
  in_use_.insert(i);
  return At(i);
}

void VersionBuffer::FreeBlock(uint8_t* host_addr, bool is_in_use) {
  int i = Index(host_addr);
  std::set<int>::iterator it_use = in_use_.find(i);
  std::set<int>::iterator it_backup = backup_.find(i);
  if (is_in_use) {
    assert(it_use != in_use_.end());
    in_use_.erase(it_use);
    assert(it_backup == backup_.end());
  } else {
    assert(it_backup != backup_.end());
    backup_.erase(it_backup);
    assert(it_use == in_use_.end());
  }
  free_.insert(i);
}

void VersionBuffer::PinBlock(uint8_t* host_addr) {
  int i = Index(host_addr);
  assert(free_.find(i) == free_.end());
  std::set<int>::iterator it = in_use_.find(i);
  assert(it != in_use_.end());
  in_use_.erase(it);
  backup_.insert(i);
}

