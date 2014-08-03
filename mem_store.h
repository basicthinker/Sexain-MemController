// mem_store.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_MEM_STORE_H_
#define SEXAIN_MEM_STORE_H_

#include <cstdint>
#include <cassert>

class MemStore {
 public:
  virtual void MemCopy(uint64_t direct_addr, uint64_t mach_addr, int size) = 0;
  virtual void MemSwap(uint64_t direct_addr, uint64_t mach_addr, int size) = 0;
 
  virtual void OnATTOp() { }
  virtual void OnBufferOp() { }
  virtual uint64_t GetReadLatency(uint64_t mach_addr, bool dram) { return -1; }
  virtual uint64_t GetWriteLatency(uint64_t mach_addr, bool dram) { return -1; }

  virtual void OnNVMRead(uint64_t mach_addr, int size) { }
  virtual void OnNVMStore(uint64_t phy_addr, int size) { }
  virtual void OnDRAMRead(uint64_t mach_addr, int size) { }
  virtual void OnDRAMStore(uint64_t phy_addr, int size) { }

  virtual void OnEpochEnd() { }
  virtual void OnATTWriteHit(int state) { }
  virtual void OnATTWriteMiss(int state) { }

  virtual void OnCacheRegister() { }
  virtual void ckBusUtilAdd(uint64_t bytes) { }
  virtual void ckNVMWrite() { }
  virtual void ckDRAMWrite() { }
  virtual void ckDRAMWriteHit() { }
};

#endif // SEXAIN_MEM_STORE_H_

