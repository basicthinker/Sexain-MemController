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

typedef int64_t Tag; // never negative
typedef uint64_t Addr;

struct ATTEntry {
  enum State {
    CLEAN = 0,
    LOAN,
    FREE,
    DIRTY, // max queue index
    HIDDEN,
    TEMP,
  };

  Tag phy_tag;
  Addr mach_base;
  IndexNode queue_node;
  State state;
};

class AddrTransTable : public IndexArray {
 public:
  AddrTransTable(int length, int block_bits);

  std::pair<int, Addr> Lookup(Tag phy_tag);
  void Setup(Tag phy_tag, Addr mach_base, ATTEntry::State state);
  void ShiftState(int index, ATTEntry::State state);
  void Reset(int index, Addr new_base, ATTEntry::State new_state);
  void VisitQueue(ATTEntry::State state, QueueVisitor* visitor);

  const ATTEntry& At(int i) const;
  bool Contains(Addr phy_addr) const;
  bool IsEmpty(ATTEntry::State state) const;
  int GetLength(ATTEntry::State state) const;
  int GetFront(ATTEntry::State state) const;

  Tag ToTag(Addr addr) const { return Tag(addr >> block_bits_); }
  Addr ToAddr(Tag tag) const { return Addr(tag) << block_bits_; }
  Addr Translate(Addr phy_addr, Addr mach_base) const;

  int length() const { return length_; }
  int block_size() const { return 1 << block_bits_; }
  int block_bits() const { return block_bits_; }
  IndexNode& operator[](int i) { return entries_[i].queue_node; }

 private:
  const int length_;
  const int block_bits_;
  const Addr block_mask_;
  std::unordered_map<Tag, int> tag_index_;
  std::vector<ATTEntry> entries_;
  std::vector<IndexQueue> queues_;
};

inline AddrTransTable::AddrTransTable(int length, int block_bits) :
    length_(length), block_bits_(block_bits), block_mask_(block_size() - 1),
    entries_(length_), queues_(ATTEntry::DIRTY + 1, *this) {
  for (int i = 0; i < length_; ++i) {
    queues_[ATTEntry::FREE].PushBack(i);
  }
}

inline const ATTEntry& AddrTransTable::At(int i) const {
  assert(i >= 0 && i < length_);
  return entries_[i];
}

inline bool AddrTransTable::Contains(Addr phy_addr) const {
  return tag_index_.find(Tag(phy_addr)) != tag_index_.end();
}

inline void AddrTransTable::VisitQueue(ATTEntry::State state,
    QueueVisitor* visitor) {
  assert(state <= ATTEntry::DIRTY);
  queues_[state].Accept(visitor);
}

inline bool AddrTransTable::IsEmpty(ATTEntry::State state) const {
  assert(state <= ATTEntry::DIRTY);
  return queues_[state].Empty();
}

inline int AddrTransTable::GetLength(ATTEntry::State state) const {
  assert(state <= ATTEntry::DIRTY);
  return queues_[state].length();
}

inline int AddrTransTable::GetFront(ATTEntry::State state) const {
  assert(state <= ATTEntry::DIRTY);
  return queues_[state].Front();
}
 
inline Addr AddrTransTable::Translate(
    Addr phy_addr, Addr mach_base) const {
  return mach_base + (phy_addr & block_mask_);
}

#endif // SEXAIN_ADDR_TRANS_TABLE_H_
