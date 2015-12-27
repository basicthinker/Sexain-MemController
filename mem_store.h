// mem_store.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_MEM_STORE_H_
#define SEXAIN_MEM_STORE_H_

#include <cstdint>
#include <cassert>

namespace thynvm {

class MemStore {
 public:
  virtual void MemCopy(uint64_t direct_addr, uint64_t mach_addr, int size) = 0;
  virtual void MemSwap(uint64_t direct_addr, uint64_t mach_addr, int size) = 0;
 
  virtual void OnATTOp() { }
  virtual void OnBufferOp() { }
  virtual uint64_t GetReadLatency(uint64_t mach_addr, bool dram) { return -1; }
  virtual uint64_t GetWriteLatency(uint64_t mach_addr, bool dram) { return -1; }
};

}  // namespace thynvm

#endif // SEXAIN_MEM_STORE_H_

