// addr_trans_controller.h
// Copyright (c) 2014 Jinglei Ren <jinglei.ren@stanzax.org>

#ifndef SEXAIN_ADDR_TRANS_CONTROLLER_H_
#define SEXAIN_ADDR_TRANS_CONTROLLER_H_

#include <cassert>
#include "addr_trans_table.h"

class AddrTransController {
 public:
  AddrTransController(uint64_t dram_size, uint64_t nvm_size,
    AddrTransTable& blk_tbl, AddrTransTable& pg_tbl);
  AddrTransController(uint64_t phy_limit, AddrTransTable& blk_tbl,
    AddrTransTable& pg_tbl, uint64_t dram_size);

  uint64_t LoadAddr(uint64_t phy_addr);
  uint64_t StoreAddr(uint64_t phy_addr);
  void NewEpoch();

  int cache_block_size() const { return block_table_.block_size(); }
  uint64_t phy_limit() const { return phy_limit_; }
  uint64_t dram_size() const { return dram_size_; }
  uint64_t nvm_size() const { return nvm_size_; }
 private:
  const uint64_t dram_size_;
  const uint64_t nvm_size_;
  const uint64_t phy_limit_;

  AddrTransTable& block_table_;
  AddrTransTable& page_table_;
};

inline void AddrTransController::NewEpoch() {
  page_table_.NewEpoch();
  block_table_.NewEpoch();
}

#endif // SEXAIN_ADDR_TRANS_CONTROLLER_H_

