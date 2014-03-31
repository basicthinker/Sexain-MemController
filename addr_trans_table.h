// addr_trans_table.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_ADDR_TRANS_TABLE_H_
#define SEXAIN_ADDR_TRANS_TABLE_H_

#include <cerrno>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <iostream>
#include "index_queue.h"
#include "shadow_tag_mapper.h"
#include "mem_store.h"

struct ATTEntry {
  uint64_t phy_tag;
  uint64_t mach_tag;
  IndexNode queue_node;
  bool dirty;
  bool valid;

  ATTEntry() : dirty(false), valid(false) {
    queue_node.first = queue_node.second = -EINVAL;
  }
};

enum ATTOperation {
  EPOCH = 0,
  SHRINK,
  WRBACK,
  REGWR,
};

class AddrTransTable : public IndexArray {
 public:
  AddrTransTable(int block_bits, ShadowTagMapper& mapper, MemStore& mem);

  enum ATTOperation Probe(uint64_t phy_addr);
  uint64_t LoadAddr(uint64_t phy_addr);
  uint64_t StoreAddr(uint64_t phy_addr);
  void NewEpoch();

  uint64_t Tag(uint64_t addr) const { return addr >> block_bits_; }
  void RevokeTag(uint64_t tag);

  uint64_t image_floor() const;
  void set_image_floor(uint64_t addr);

  int block_size() const { return 1 << block_bits_; }
  int block_bits() const { return block_bits_; }
  int length() const { return mapper_.length(); }
  int image_size() const { return block_size() * length(); }
  IndexNode& operator[](int i) { return entries_[i].queue_node; }

 private:
  uint64_t Addr(uint64_t tag) const { return tag << block_bits_; }
  uint64_t Trans(uint64_t phy_addr, uint64_t mach_tag);

  const int block_bits_;
  const uint64_t block_mask_;
  ShadowTagMapper& mapper_;
  MemStore& mem_store_;
  std::unordered_map<uint64_t, int> tag_index_;
  std::vector<ATTEntry> entries_;
  std::vector<IndexQueue> queues_;
};

inline AddrTransTable::AddrTransTable(int block_bits,
    ShadowTagMapper& mapper, MemStore& mem) :

    block_bits_(block_bits), block_mask_(block_size() - 1), mapper_(mapper),
    mem_store_(mem), entries_(length()), queues_(2, *this) {

  for (int i = 0; i < length(); ++i) {
    queues_[0].PushBack(i); // clean queue
  }
}

inline void AddrTransTable::NewEpoch() {
  assert(IndexIntersection(queues_[0], queues_[1]).size() == 0);
  while (!queues_[1].Empty()) {
    int i = queues_[1].PopFront();
    entries_[i].dirty = false;
    queues_[0].PushBack(i);
  }
  assert(queues_[0].GetLength() == entries_.size());
}

inline uint64_t AddrTransTable::image_floor() const {
  assert(mapper_.floor() >= 0);
  return Addr(mapper_.floor());
}

inline void AddrTransTable::set_image_floor(uint64_t addr) {
  assert((addr & block_mask_) == 0);
  mapper_.set_floor(Tag(addr));
}

inline uint64_t AddrTransTable::Trans(uint64_t phy_addr, uint64_t mach_tag) {
  return Addr(mach_tag) + (phy_addr & block_mask_);
}

#endif // SEXAIN_ADDR_TRANS_TABLE_H_

