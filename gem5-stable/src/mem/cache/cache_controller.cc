// cache_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "mem/cache/cache_controller.h"

void CacheController::DirtyBlock(uint64_t addr, int size) {
  uint64_t begin = addr & ~block_mask();
  uint64_t end = (addr + size - 1) & ~block_mask();
  for (uint64_t a = begin; a <= end; a += block_size()) {
    if (!memory_->isStatic(a)) {
      ++blocks_[a];
    }
  }

  if (blocks_.size() == att_length_) {
    cache_->writebackAllTiming();
    blocks_.clear();
  }
}

CacheController* CacheControllerParams::create() {
  return new CacheController(this);
}
