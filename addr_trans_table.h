// addr_trans_table.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_ADDR_TRANS_TABLE_H_
#define SEXAIN_ADDR_TRANS_TABLE_H_

#include <cerrno>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <initializer_list>
#include "index_queue.h"


struct ATTEntry {
  enum State {
    DIRTY = 0,
    TEMP,
    CLEAN,
    FREE, // always placed the last
  };

  enum TempType {
    REG_TEMP = 0x00000000,
    MACH_RESET = 0x00000001,
    PHY_DRAM = 0x00000002,
    NON_TEMP = 0xfffffff0,
  };

  uint64_t phy_tag;
  uint64_t mach_base;
  IndexNode queue_node;
  State state;
  uint32_t flag;

  ATTEntry() : state(FREE) {
    phy_tag = mach_base = -1;
    queue_node.first = queue_node.second = -EINVAL;
    flag = 0;
  }

  bool Test(uint32_t mask) const { return flag & mask; }
  void Set(uint32_t mask) { flag |= mask; }
  void Clear(uint32_t mask) { flag &= ~mask; }
};

class AddrTransTable : public IndexArray {
 public:
  AddrTransTable(int length, int block_bits);

  uint64_t Lookup(uint64_t phy_tag, int* index);
  void Setup(uint64_t phy_tag, uint64_t mach_base,
      ATTEntry::State state, uint32_t flag = ATTEntry::NON_TEMP);
  void Revoke(uint64_t phy_tag);
  ///
  /// Replace an existing clean mapping with the specified one.
  /// @return the replaced clean mapping
  ///
  std::pair<uint64_t, uint64_t> Replace(uint64_t phy_tag, uint64_t mach_base,
      ATTEntry::State state, uint32_t flag = ATTEntry::NON_TEMP);
  void Reset(int index, uint64_t mach_base,
      ATTEntry::State state, uint32_t flag_mask = ATTEntry::NON_TEMP);
  void FreeEntry(int index);
  void VisitQueue(ATTEntry::State state, QueueVisitor* visitor);
  int CleanDirtyQueue();
  int FreeQueue(ATTEntry::State state);

  const ATTEntry& At(int i) const;
  bool Contains(uint64_t phy_addr) const;
  bool IsEmpty(ATTEntry::State state) const { return queues_[state].Empty(); }
  int GetLength(ATTEntry::State state) const { return queues_[state].length(); }
  int GetLength(std::initializer_list<ATTEntry::State> states) const;
  int GetFront(ATTEntry::State state) const { return queues_[state].Front(); }

  uint64_t Tag(uint64_t addr) const { return addr >> block_bits_; }
  uint64_t Addr(uint64_t tag) const { return tag << block_bits_; }
  uint64_t Translate(uint64_t phy_addr, uint64_t mach_base) const;

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
    entries_(length_), queues_((int)ATTEntry::FREE + 1, *this) {
  for (int i = 0; i < length_; ++i) {
    queues_[ATTEntry::FREE].PushBack(i);
  }
}

inline const ATTEntry& AddrTransTable::At(int i) const {
  assert(i >= 0 && i < length_);
  return entries_[i];
}

inline bool AddrTransTable::Contains(uint64_t phy_addr) const {
  return tag_index_.find(Tag(phy_addr)) != tag_index_.end();
}

inline int AddrTransTable::GetLength(
    std::initializer_list<ATTEntry::State> states) const {
  int len = 0;
  for (std::initializer_list<ATTEntry::State>::iterator it = states.begin();
      it != states.end(); ++it) {
    len += GetLength(*it);
  }
  return len;
}

inline void AddrTransTable::VisitQueue(ATTEntry::State state,
    QueueVisitor* visitor) {
  queues_[state].Accept(visitor);
}
 
inline uint64_t AddrTransTable::Translate(
    uint64_t phy_addr, uint64_t mach_base) const {
  return mach_base + (phy_addr & block_mask_);
}

#endif // SEXAIN_ADDR_TRANS_TABLE_H_

