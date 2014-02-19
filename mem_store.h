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
};

class TraceMemStore : public MemStore {
 public:
  void OnDirectWrite(uint64_t phy_tag, uint64_t mach_tag, int bits) {
    std::cout << std::hex;
    std::cout << "[Info] MemStore directly writes: "
        << phy_tag << " => " << mach_tag << " (" << bits << ')' << std::endl;
    std::cout << std::dec; 
  }

  void OnWriteBack(uint64_t phy_tag, uint64_t mach_tag, int bits) {
    assert(phy_tag != mach_tag);
    std::cout << std::hex;
    std::cout << "[Info] MemStore writes back from "
        << mach_tag << " to " << phy_tag << " (" << bits << ')' << std::endl;
    std::cout << std::dec;
  }

  void OnOverwrite(uint64_t phy_tag, uint64_t mach_tag, int bits) {
    assert(phy_tag != mach_tag);
    std::cout << std::hex;
    std::cout << "[Info] MemStore overwrites: "
        << phy_tag << " => " << mach_tag << " (" << bits << ')' << std::endl;
    std::cout << std::dec; 
  }

  void OnShrink(uint64_t phy_tag, uint64_t mach_tag, int bits) {
    assert(phy_tag != mach_tag);
    std::cout << std::hex;
    std::cout << "[Info] MemStore shrinks: "
        << phy_tag << " => " << mach_tag << " (" << bits << ')' << std::endl;
    std::cout << std::dec;
  }

  void OnEpochEnd(int bits) {
    std::cout << "[Info] MemStore meets epoch end (" << bits << ")."
        << std::endl;
  }
};

#endif // SEXAIN_MEM_STORE_H_

