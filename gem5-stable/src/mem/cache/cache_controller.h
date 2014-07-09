// cache_controller.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_CACHE_CONTROLLER_H_
#define SEXAIN_CACHE_CONTROLLER_H_

#include <vector>
#include <map>
#include <cassert>

#include "mem/cache/base.hh"
#include "params/CacheController.hh"

class CacheController : public SimObject {
 public:
  CacheController(const CacheControllerParams* p);

  void RegisterCache(BaseCache* const cache);
  void DirtyBlock(uint64_t addr, int size);

  int block_size() const { return block_size_; }
  int block_mask() const { return block_mask_; }
  int att_length() const { return att_length_; }

 private:

  void WritebackAll();

  std::vector<BaseCache*> caches_;
  std::map<uint64_t, int> blocks_;
  const int block_bits_;
  const int att_length_;
  uint64_t block_size_;
  uint64_t block_mask_;
};

inline CacheController::CacheController(const CacheControllerParams* p) :
    SimObject(p), block_bits_(p->block_bits), att_length_(p->att_length) {
  assert(block_bits_ > 0 && att_length_ > 0);
  block_size_ = (1 << block_bits_);
  block_mask_ = block_size_ - 1;
}

inline void CacheController::RegisterCache(BaseCache* const cache) {
  caches_.push_back(cache);
  std::cout << "# registered cache(s): " << caches_.size() << std::endl;
}

inline void CacheController::DirtyBlock(uint64_t addr, int size) {
  uint64_t begin = addr & ~block_mask();
  uint64_t end = (addr + size - 1) & ~block_mask();
  for (uint64_t a = begin; a <= end; a += block_size()) {
    ++blocks_[a];
  }

  if (blocks_.size() == att_length_) {
    WritebackAll();
  }
}

inline void CacheController::WritebackAll() {
  for(std::vector<BaseCache*>::iterator it = caches_.begin();
      it != caches_.end(); ++it) {
    (*it)->memWritebackTiming();
  }
  blocks_.clear();
}

#endif // SEXAIN_CACHE_CONTROLLER_H_

