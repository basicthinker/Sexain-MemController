// mem_store.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_MEM_STORE_H_
#define SEXAIN_MEM_STORE_H_

#include <iostream>
#include <cstdint>
#include <cassert>

class MemStore {
 public:
  virtual void NVMMove(uint64_t phy_addr, uint64_t mach_addr, int size) = 0;
  virtual void NVMSwap(uint64_t phy_addr, uint64_t mach_addr, int size) = 0;
  virtual void DRAMMove(uint64_t phy_addr, uint64_t mach_addr, int size) = 0;
  virtual void WriteBack(uint64_t phy_addr, uint64_t mach_addr, int size) = 0;
 
  virtual void OnATTOperate() { }
  virtual void OnBufferOperate() { }

  virtual void OnNVMRead(uint64_t mach_addr, int size) { }
  virtual void OnNVMWrite(uint64_t mach_addr, int size) { }
  virtual void OnDRAMRead(uint64_t mach_addr, int size) { }
  virtual void OnDRAMWrite(uint64_t mach_addr, int size) { }

  virtual void OnEpochEnd() { }
};

class TraceMemStore : public MemStore {
  void NVMMove(uint64_t phy_addr, uint64_t mach_addr, int size) {
    std::cout << std::hex;
    std::cout << "[Info] NVM moves from " << mach_addr << " to " << phy_addr
        << std::endl;
    std::cout << std::dec;
  }

  void NVMSwap(uint64_t phy_addr, uint64_t mach_addr, int size) {
    std::cout << std::hex;
    std::cout << "[Info] NVM swaps " << mach_addr << " and " << phy_addr
        << std::endl;
    std::cout << std::dec;
  }

  void DRAMMove(uint64_t phy_addr, uint64_t mach_addr, int size) {
    std::cout << std::hex;
    std::cout << "[Info] DRAM moves from " << mach_addr << " to " << phy_addr
        << std::endl;
    std::cout << std::dec;
  }

  void WriteBack(uint64_t phy_addr, uint64_t mach_addr, int size) {
    std::cout << std::hex;
    std::cout << "[Info] Write back from " << mach_addr << " to " << phy_addr
        << std::endl;
    std::cout << std::dec;
  }

  void OnATTOperate() {
    std::cout << "[Info] ATT operation." << std::endl;
  }

  void OnBufferOperate() {
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

