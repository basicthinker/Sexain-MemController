// mem_store.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_MEM_STORE_H_
#define SEXAIN_MEM_STORE_H_

#include <iostream>
#include <cstdint>
#include <cassert>

class MemStore {
 public:
  virtual void DoMove(uint64_t destination, uint64_t source, int size) = 0;
  virtual void DoSwap(uint64_t static_addr, uint64_t mach_addr, int size) = 0;
 
  virtual void OnATTOp() { }
  virtual void OnBufferOp() { }

  virtual void OnNVMRead(uint64_t mach_addr, int size) { }
  virtual void OnNVMWrite(uint64_t mach_addr, int size) { }
  virtual void OnDRAMRead(uint64_t mach_addr, int size) { }
  virtual void OnDRAMWrite(uint64_t mach_addr, int size) { }

  ///
  /// Before the checkpointing frame begins
  ///
  virtual void OnCheckpointing(int num_new_at, int num_new_pt) = 0;
  ///
  /// When ATT is saturated in checkpointing and the write request has to wait
  ///
  virtual void OnWaiting() = 0;

  ///
  /// Behavioral events in THNVM schemes
  ///
  virtual void OnEpochEnd() { }
  virtual void OnATTFreeSetup(uint64_t phy_addr, int state) { } 
  virtual void OnATTHideClean(uint64_t phy_addr, bool move_data) { } 
  virtual void OnATTResetClean(uint64_t phy_addr, bool move_data) { } 
  virtual void OnATTFreeClean(uint64_t phy_addr, bool move_data) { } 
  virtual void OnATTFreeLoan(uint64_t phy_addr, bool move_data) { }

  virtual void OnCacheRegister() { }
  virtual void OnCacheFlush(int blocks, int pages) { }
};

class TraceMemStore : public MemStore {
  void DoMove(uint64_t destination, uint64_t source, int size) {
    std::cout << std::hex;
    std::cout << "[Info] Mem moves from " << source << " to " << destination
        << std::endl;
    std::cout << std::dec;
  }

  void DoSwap(uint64_t static_addr, uint64_t mach_addr, int size) {
    std::cout << std::hex;
    std::cout << "[Info] NVM swaps " << mach_addr << " and " << static_addr
        << std::endl;
    std::cout << std::dec;
  }

  void OnATTOp() {
    std::cout << "[Info] ATT operation." << std::endl;
  }

  void OnBufferOpe() {
    std::cout << "[Info] VersionBuffer operation." << std::endl;
  }

  void OnNVMRead(uint64_t mach_addr, int size) {
    std::cout << std::hex;
    std::cout << "[Info] MemStore reads NVM: " << mach_addr << std::endl;
    std::cout << std::dec;
  }

  void OnNVMWrite(uint64_t mach_addr, int size) {
    std::cout << std::hex;
    std::cout << "[Info] MemStore writes to NVM: " << mach_addr << std::endl;
    std::cout << std::dec;
  }

  void OnDRAMRead(uint64_t mach_addr, int size) {
    std::cout << std::hex;
    std::cout << "[Info] MemStore reads DRAM: " << mach_addr << std::endl;
    std::cout << std::dec;
  }

  void OnDRAMWrite(uint64_t mach_addr, int size) {
    std::cout << std::hex;
    std::cout << "[Info] MemStore writes to DRAM: " << mach_addr << std::endl;
    std::cout << std::dec;
  }

  void OnEpochEnd() {
    std::cout << "[Info] MemStore meets epoch end." << std::endl;
  }
};

#endif // SEXAIN_MEM_STORE_H_

