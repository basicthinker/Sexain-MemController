// addr_trans_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "addr_trans_controller.h"

uint64_t AddrTransController::LoadAddr(uint64_t phy_addr) {
  assert(phy_addr < phy_limit());
  if (phy_addr < dram_size_) {
    mem_store_->OnDRAMRead(phy_addr);

    // suppose the page table is pre-modified
    uint64_t mach_addr = page_table_.LoadAddr(phy_addr);
    return mach_addr;
  } else {
    uint64_t mach_addr = block_table_.LoadAddr(phy_addr);
    mem_store_->OnNVMRead(mach_addr);
    return mach_addr;
  }
}

uint64_t AddrTransController::StoreAddr(uint64_t phy_addr) {
  assert(phy_addr < phy_limit());
  if (phy_addr < dram_size_) {
    if (page_table_.Probe(phy_addr) == EPOCH) {
      NewEpoch();
    }
    mem_store_->OnDRAMWrite(phy_addr);

    // pre-modification that should happen in the end of epoch
    uint64_t mach_addr = page_table_.StoreAddr(phy_addr);
    return mach_addr;
  } else {
    if (block_table_.Probe(phy_addr) == EPOCH) {
      NewEpoch();
    }
    uint64_t mach_addr = block_table_.StoreAddr(phy_addr);
    mem_store_->OnNVMWrite(mach_addr);
    return mach_addr;
  }
}

AddrStatus AddrTransController::Probe(uint64_t phy_addr) {
  assert(phy_addr < phy_limit());
  AddrStatus status;
  status.type = (phy_addr < dram_size_);
  status.oper = status.type ? 
      page_table_.Probe(phy_addr) : block_table_.Probe(phy_addr);
  return status;
}

void AddrTransController::NewEpoch() {
  page_table_.NewEpoch();
  block_table_.NewEpoch();
}

