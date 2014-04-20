// addr_trans_table.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_ADDR_TRANS_TABLE_H_
#define SEXAIN_ADDR_TRANS_TABLE_H_

#include <cerrno>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "index_queue.h"

enum EntryState {
  DIRTY_ENTRY = 0,
  TEMP_ENTRY,
  CLEAN_ENTRY,
  FREE_ENTRY, // always placed the last
};

struct ATTEntry {
  uint64_t phy_tag;
  uint64_t mach_addr;
  IndexNode queue_node;
  EntryState state;
  uint32_t flag;

  ATTEntry() : state(FREE_ENTRY) {
    phy_tag = mach_addr = -1;
    queue_node.first = queue_node.second = -EINVAL;
    flag = 0;
  }

  bool Test(uint32_t mask) { return flag & mask; }
  void Set(uint32_t mask) { flag |= mask; }
  void Clear(uint32_t mask) { flag &= ~mask; }

  static const uint32_t MACH_RESET = 0x00000001;
  static const uint32_t PHY_DRAM = 0x00000002;
};

class EntryVisitor {
 public:
  virtual void Visit(ATTEntry& entry) = 0;
};

class AddrTransTable : public IndexArray {
 public:
  AddrTransTable(int length, int block_bits);

  uint64_t Lookup(uint64_t phy_tag, int* index);
  void Setup(uint64_t phy_tag, uint64_t mach_addr,
      EntryState state, uint32_t flag_mask = 0);
  void Revoke(uint64_t phy_tag);
  ///
  /// Replace an existing clean mapping with the specified one.
  /// @return the replaced clean mapping
  ///
  std::pair<uint64_t, uint64_t> Replace(uint64_t phy_tag, uint64_t mach_addr,
      EntryState state, uint32_t flag_mask = 0);
  void Reset(int index, uint64_t mach_addr,
      EntryState state, uint32_t flag_mask = 0);
  void FreeEntry(int index);
  void VisitQueue(EntryState state, EntryVisitor* visitor);
  int CleanDirtyQueue();
  int FreeQueue(EntryState state);

  const ATTEntry& At(int i);
  bool IsEmpty(EntryState state) { return queues_[state].Empty(); }
  int GetLength(EntryState state) { return queues_[state].length(); }

  uint64_t Tag(uint64_t addr) { return addr >> block_bits_; }
  uint64_t Addr(uint64_t tag) { return tag << block_bits_; }
  uint64_t Translate(uint64_t phy_addr, uint64_t mach_addr);

  int length() const { return length_; }
  int block_size() const { return 1 << block_bits_; }
  int block_bits() const { return block_bits_; }
  IndexNode& operator[](int i) { return entries_[i].queue_node; }

 private:
  const int length_;
  const int block_bits_;
  const uint64_t block_mask_;
  std::unordered_map<uint64_t, int> tag_index_;
  std::vector<ATTEntry> entries_;
  std::vector<IndexQueue> queues_;
};

inline AddrTransTable::AddrTransTable(int length, int block_bits) :
    length_(length), block_bits_(block_bits), block_mask_(block_size() - 1),
    entries_(length_), queues_((int)FREE_ENTRY + 1, *this) {
  for (int i = 0; i < length_; ++i) {
    queues_[FREE_ENTRY].PushBack(i);
  }
}

inline const ATTEntry& AddrTransTable::At(int i) {
  assert(i >= 0 && i < length_);
  return entries_[i];
}
 
inline uint64_t AddrTransTable::Translate(
    uint64_t phy_addr, uint64_t mach_addr) {
  return mach_addr + (phy_addr & block_mask_);
}

#endif // SEXAIN_ADDR_TRANS_TABLE_H_

