// addr_trans_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei.ren@stanzax.org>

#include "addr_trans_controller.h"

AddrTransController::AddrTransController(uint64_t dram_size, uint64_t nvm_size, 
    AddrTransTable& blk_tbl, AddrTransTable& pg_tbl): 
    
    dram_size_(dram_size), nvm_size_(nvm_size), 
    block_table_(blk_tbl), page_table_(pg_tbl) { 

  set_phy_limit(nvm_size_ - blk_tbl.image_size() - pg_tbl.image_size()); 
  assert(phy_limit() >= dram_size_); 
  page_table_.set_image_floor(phy_limit() + dram_size_); 
  block_table_.set_image_floor(dram_size_ + nvm_size_ - blk_tbl.image_size());
  assert(page_table_.image_floor() + page_table_.image_size() ==
      block_table_.image_floor()); 
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

