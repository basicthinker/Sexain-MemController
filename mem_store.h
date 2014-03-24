// mem_store.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_MEM_STORE_H_
#define SEXAIN_MEM_STORE_H_

#include <iostream>
#include <cstdint>
#include <cassert>

class MemStore {
 public:
  // When a new entry is created
  virtual void OnNewMapping(uint64_t phy_tag, uint64_t mach_tag, int bits) = 0;
  // When no entry is hit and no mapping is evicted
  virtual void OnDirectWrite(uint64_t phy_tag, uint64_t mach_tag,
      int bits) = 0;
  // When no entry is hit but a mapping is evicted
  virtual void OnWriteBack(uint64_t phy_tag, uint64_t mach_tag, int bits) = 0;

  // When a dirty entry is hit
  virtual void OnOverwrite(uint64_t phy_tag, uint64_t mach_tag, int bits) = 0;
  // When a clean entry is hit
  virtual void OnShrink(uint64_t phy_tag, uint64_t mach_tag, int bits) = 0;

  // When a new epoch is to start
  virtual void OnEpochEnd(int bits) = 0;

  virtual void OnNVMRead(uint64_t mach_addr) = 0;
  virtual void OnNVMWrite(uint64_t mach_addr) = 0;
  virtual void OnDRAMRead(uint64_t mach_addr) = 0;
  virtual void OnDRAMWrite(uint64_t mach_addr) = 0;
};

class TraceMemStore : public MemStore {
 public:
  void OnNewMapping(uint64_t phy_tag, uint64_t mach_tag, int bits) {
    std::cout << std::hex;
    std::cout << "[Info] MemStore creates new mapping: "
        << phy_tag << " => " << mach_tag
        << std::dec << " (" << bits << ')' << std::endl;
  }

  void OnDirectWrite(uint64_t phy_tag, uint64_t mach_tag, int bits) {
    std::cout << std::hex;
    std::cout << "[Info] MemStore directly writes: "
        << phy_tag << " => " << mach_tag
        << std::dec << " (" << bits << ')' << std::endl;
  }

  void OnWriteBack(uint64_t phy_tag, uint64_t mach_tag, int bits) {
    assert(phy_tag != mach_tag);
    std::cout << std::hex;
    std::cout << "[Info] MemStore writes back from "
        << mach_tag << " to " << phy_tag
        << std::dec << " (" << bits << ')' << std::endl;
  }

  void OnOverwrite(uint64_t phy_tag, uint64_t mach_tag, int bits) {
    assert(phy_tag != mach_tag);
    std::cout << std::hex;
    std::cout << "[Info] MemStore overwrites: "
        << phy_tag << " => " << mach_tag
        << std::dec << " (" << bits << ')' << std::endl;
  }

  void OnShrink(uint64_t phy_tag, uint64_t mach_tag, int bits) {
    assert(phy_tag != mach_tag);
    std::cout << std::hex;
    std::cout << "[Info] MemStore shrinks: "
        << phy_tag << " => " << mach_tag
        << std::dec << " (" << bits << ')' << std::endl;
  }

  void OnEpochEnd(int bits) {
    std::cout << "[Info] MemStore meets epoch end (" << bits << ")."
        << std::endl;
  }

  void OnNVMRead(uint64_t mach_addr) {
    std::cout << std::hex;
    std::cout << "[Info] MemStore reads NVM: " << mach_addr << std::endl;
    std::cout << std::dec;
  }

  void OnNVMWrite(uint64_t mach_addr) {
    std::cout << std::hex;
    std::cout << "[Info] MemStore writes to NVM: " << mach_addr << std::endl;
    std::cout << std::dec;
  }

  void OnDRAMRead(uint64_t mach_addr) {
    std::cout << std::hex;
    std::cout << "[Info] MemStore reads DRAM: " << mach_addr << std::endl;
    std::cout << std::dec;
  }

  void OnDRAMWrite(uint64_t mach_addr) {
    std::cout << std::hex;
    std::cout << "[Info] MemStore writes to DRAM: " << mach_addr << std::endl;
    std::cout << std::dec;
  }
};

#endif // SEXAIN_MEM_STORE_H_

