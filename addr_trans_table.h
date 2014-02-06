// addr_trans_table.h
// Copyright (c) 2014 Jinglei Ren <jinglei.ren@stanzax.org>

#ifndef SEXAIN_ADDR_TRANS_TABLE_H_
#define SEXAIN_ADDR_TRANS_TABLE_H_

#include <cerrno>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <iostream>
#include "index_queue.h"

struct AT2Entry {
  uint64_t phy_addr;
  uint64_t mach_addr;
  IndexNode queue_node;
  bool dirty;
  bool valid;

  AT2Entry() : dirty(false), valid(false) {
    queue_node.first = queue_node.second = -EINVAL;
  }
};

class AddrTransTable : public IndexArray {
 public:
  AddrTransTable(int size, int bits);
  uint64_t LoadAddr(uint64_t phy_addr);
  uint64_t StoreAddr(uint64_t phy_addr);
  IndexNode& operator[](int i) { return entries_[i].queue_node; }
 protected:
  virtual void NewEpoch();
  virtual void WriteBack(const AT2Entry& entry);
  virtual uint64_t BufferAddr();
 private:
  const int block_bits_;
  std::unordered_map<uint64_t, int> trans_index_;
  std::vector<AT2Entry> entries_;
  std::vector<IndexQueue> queues_;
};

AddrTransTable::AddrTransTable(int size, int bits) :
    block_bits_(bits), entries_(size), queues_(2, *this) {
  for (int i = 0; i < size; ++i) {
    queues_[0].PushBack(i); // clean queue
  }
}

uint64_t AddrTransTable::LoadAddr(uint64_t phy_addr) {
  std::unordered_map<uint64_t, int>::iterator it = trans_index_.find(phy_addr);
  if (it == trans_index_.end()) { // not hit
    return phy_addr;
  } else {
    AT2Entry& entry = entries_[it->second];
    assert(entry.valid && entry.phy_addr == phy_addr);
    queues_[entry.dirty].Remove(it->second);
    queues_[entry.dirty].PushBack(it->second);
    return entry.mach_addr;
  }
}

uint64_t AddrTransTable::StoreAddr(uint64_t phy_addr) {
  std::unordered_map<uint64_t, int>::iterator it = trans_index_.find(phy_addr);
  if (it == trans_index_.end()) { // not hit
    if (queues_[0].Empty()) NewEpoch();
    int i = queues_[0].PopFront(); // clean queue
    AT2Entry& entry = entries_[i];
    if (entry.valid) {
      WriteBack(entry);
      trans_index_.erase(entries_[i].phy_addr);
    }

    entries_[i].phy_addr = phy_addr;
    entries_[i].mach_addr = BufferAddr();
    entries_[i].dirty = true;
    entries_[i].valid = true;

    queues_[1].PushBack(i); // dirty queue
    trans_index_[phy_addr] = i;
    return entries_[i].mach_addr;

  } else { // hit
    AT2Entry& entry = entries_[it->second];
    assert(entry.valid && entry.phy_addr == phy_addr);
    queues_[entry.dirty].Remove(it->second);
    if (entry.dirty) {
      queues_[1].PushBack(it->second);
      return entry.mach_addr;
    } else {
      trans_index_.erase(it);
      entry.valid = false;
      queues_[0].PushFront(it->second);
      return phy_addr;
    }
  }
}

void AddrTransTable::NewEpoch() {
  assert(IndexIntersection(queues_[0], queues_[1]).size() == 0);
  while (!queues_[1].Empty()) {
    int i = queues_[1].PopFront();
    entries_[i].dirty = false;
    queues_[0].PushBack(i);
  }
  assert(queues_[0].Indexes().size() == entries_.size());
}

void AddrTransTable::WriteBack(const AT2Entry& entry) {
  std::cerr << std::hex;
  std::cerr << "[Info] AddrTransTable::WriteBack: "
      << entry.mach_addr << " => " << entry.phy_addr << std::endl;
  return;
}

uint64_t AddrTransTable::BufferAddr() {
  return 0xbadcafe;
}

#endif // SEXAIN_ADDR_TRANS_TABLE_H_

