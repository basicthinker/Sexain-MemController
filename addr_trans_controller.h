// addr_trans_controller.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_ADDR_TRANS_CONTROLLER_H_
#define SEXAIN_ADDR_TRANS_CONTROLLER_H_

#include <cassert>
#include "mem_store.h"
#include "addr_trans_table.h"

class AddrTransController {
 public:
  AddrTransController(uint64_t dram_size, uint64_t nvm_size,
    AddrTransTable& blk_tbl, AddrTransTable& pg_tbl, MemStore* mem_store);
  AddrTransController(uint64_t phy_range, AddrTransTable& blk_tbl,
    AddrTransTable& pg_tbl, uint64_t dram_size, MemStore* mem_store);

  uint64_t LoadAddr(uint64_t phy_addr);
  uint64_t StoreAddr(uint64_t phy_addr);
  void NewEpoch();

  int cache_block_size() const { return block_table_.block_size(); }
  uint64_t phy_limit() const { return phy_limit_; }
  uint64_t dram_size() const { return dram_size_; }
  uint64_t nvm_size() const { return nvm_size_; }
  MemStore* mem_store() const { return mem_store_; }
  void set_mem_store(MemStore* mem_store) { mem_store_ = mem_store; }
 private:
  const uint64_t dram_size_;
  const uint64_t nvm_size_;
  AddrTransTable& block_table_;
  AddrTransTable& page_table_;
  const uint64_t phy_limit_;
  MemStore* mem_store_;
};

inline void AddrTransController::NewEpoch() {
  page_table_.NewEpoch();
  block_table_.NewEpoch();
}

#endif // SEXAIN_ADDR_TRANS_CONTROLLER_H_

