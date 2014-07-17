// cache_controller.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_CACHE_CONTROLLER_H_
#define SEXAIN_CACHE_CONTROLLER_H_

#include <vector>
#include <map>
#include <cassert>

#include "mem/cache/base.hh"
#include "mem/abstract_mem.hh"
#include "params/CacheController.hh"

class CacheController : public SimObject {
 public:
  CacheController(const CacheControllerParams* p);

  void RegisterCache(BaseCache* const cache);
  void DirtyBlock(uint64_t addr, int size);

  int block_size() const { return block_size_; }
  uint64_t block_mask() const { return block_mask_; }
  int att_length() const { return att_length_; }

 private:
  AbstractMemory* memory_;
  BaseCache* cache_;

  std::map<uint64_t, int> blocks_;
  const int block_bits_;
  const int att_length_;
  int block_size_;
  uint64_t block_mask_;
};

inline CacheController::CacheController(const CacheControllerParams* p) :
    SimObject(p), memory_(p->memory), cache_(NULL),
    block_bits_(p->block_bits), att_length_(p->att_length) {

  assert(memory_);

  assert(block_bits_ > 0 && att_length_ > 0);
  block_size_ = (1 << block_bits_);
  block_mask_ = block_size_ - 1;
}

inline void CacheController::RegisterCache(BaseCache* const cache) {
  assert(!cache_);
  cache_ = cache;
  memory_->OnCacheRegister();
}

#endif // SEXAIN_CACHE_CONTROLLER_H_

