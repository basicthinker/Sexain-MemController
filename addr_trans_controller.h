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

  uint64_t LoadAddr(uint64_t phy_addr);
  uint64_t StoreAddr(uint64_t phy_addr);
  void NewEpoch();

  uint64_t phy_limit() const { return phy_limit_; }

 private:
  void set_phy_limit(uint64_t limit) { phy_limit_ = limit; }

  const uint64_t dram_size_;
  const uint64_t nvm_size_;
  uint64_t phy_limit_;

  AddrTransTable& block_table_;
  AddrTransTable& page_table_;
};

inline void AddrTransController::NewEpoch() {
  page_table_.NewEpoch();
  block_table_.NewEpoch();
}

#endif // SEXAIN_ADDR_TRANS_CONTROLLER_H_

