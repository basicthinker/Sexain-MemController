// addr_trans_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei.ren@stanzax.org>

#include "addr_trans_controller.h"
#include <iostream> //DEBUG

// Space partition (low -> high):
// DRAM direct |(DRAM size)|| NVM direct |(phy_limit)||
// ATT buffer || DRAM backup || Page table buffer 
AddrTransController::AddrTransController(uint64_t dram_size, uint64_t nvm_size, 
    AddrTransTable& blk_tbl, AddrTransTable& pg_tbl): 
    
    dram_size_(dram_size), nvm_size_(nvm_size), 
    block_table_(blk_tbl), page_table_(pg_tbl),
    phy_limit_(nvm_size - blk_tbl.image_size() - pg_tbl.image_size()) {
 
  assert(phy_limit_ >= dram_size_); 
  block_table_.set_image_floor(phy_limit());
  page_table_.set_image_floor(dram_size_ + nvm_size_ - pg_tbl.image_size()); 
  assert(block_table_.image_floor() + block_table_.image_size() ==
      page_table_.image_floor() - dram_size_); 
}

AddrTransController::AddrTransController(uint64_t phy_range, 
    AddrTransTable& blk_tbl, AddrTransTable& pg_tbl, uint64_t dram_size):

    AddrTransController(dram_size,
        phy_range + blk_tbl.image_size() + pg_tbl.image_size(),
        blk_tbl, pg_tbl) {
 
  assert(phy_limit() == phy_range); 
}

uint64_t AddrTransController::LoadAddr(uint64_t phy_addr) {
  assert(phy_addr < phy_limit());
  if (phy_addr < dram_size_) {
    return phy_addr;
  } else {
    return block_table_.LoadAddr(phy_addr);
  }
}

uint64_t AddrTransController::StoreAddr(uint64_t phy_addr) {
  assert(phy_addr < phy_limit());
  if (phy_addr < dram_size_) {
    if (!page_table_.Probe(phy_addr)) {
      NewEpoch();
    }
    page_table_.StoreAddr(phy_addr);
    return phy_addr;
  } else {
    if (!block_table_.Probe(phy_addr)) {
      NewEpoch();
    }
    return block_table_.StoreAddr(phy_addr);
  }
}

