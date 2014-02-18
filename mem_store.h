// mem_store.h
// Copyright (c) 2014 Jinglei Ren <jinglei.ren@stanzax.org>

#ifndef SEXAIN_MEM_STORE_H_
#define SEXAIN_MEM_STORE_H_

#include <iostream>
#include <cstdint>
#include <cassert>

class MemStore {
 public:
  // When no entry is hit and no mapping is evicted
  virtual void OnDirectWrite(uint64_t phy_addr, uint64_t mach_addr,
      int bits) = 0;
  // When no entry is hit but a mapping is evicted
  virtual void OnWriteBack(uint64_t phy_addr, uint64_t mach_addr, int bits) = 0;

  // When a dirty entry is hit
  virtual void OnOverwrite(uint64_t phy_addr, uint64_t mach_addr, int bits) = 0;
  // When a clean entry is hit
  virtual void OnShrink(uint64_t phy_addr, uint64_t mach_addr, int bits) = 0;

  // When a new epoch is to start
  virtual void OnEpochEnd() = 0;
};

class TraceMemStore : public MemStore {
 public:
  void OnDirectWrite(uint64_t phy_addr, uint64_t mach_addr, int bits) {
    std::cout << std::hex;
    std::cout << "[Info] MemStore directly writes: "
        << phy_addr << " => " << mach_addr << " (" << bits << ')' << std::endl;
    std::cout << std::dec; 
  }

  void OnWriteBack(uint64_t phy_addr, uint64_t mach_addr, int bits) {
    assert(phy_addr != mach_addr);
    std::cout << std::hex;
    std::cout << "[Info] MemStore writes back from "
        << mach_addr << " to " << phy_addr << " (" << bits << ')' << std::endl;
    std::cout << std::dec;
  }

  void OnOverwrite(uint64_t phy_addr, uint64_t mach_addr, int bits) {
    assert(phy_addr != mach_addr);
    std::cout << std::hex;
    std::cout << "[Info] MemStore overwrites: "
        << phy_addr << " => " << mach_addr << " (" << bits << ')' << std::endl;
    std::cout << std::dec; 
  }

  void OnShrink(uint64_t phy_addr, uint64_t mach_addr, int bits) {
    assert(phy_addr != mach_addr);
    std::cout << std::hex;
    std::cout << "[Info] MemStore shrinks: "
        << phy_addr << " => " << mach_addr << " (" << bits << ')' << std::endl;
    std::cout << std::dec;
  }

  void OnEpochEnd() {
    std::cout << "[Info] MemStore meets epoch end." << std::endl;
  }
};

#endif // SEXAIN_MEM_STORE_H_

