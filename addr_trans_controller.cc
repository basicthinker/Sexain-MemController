// addr_trans_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "addr_trans_controller.h"

uint64_t AddrTransController::LoadAddr(uint64_t phy_addr) {
  assert(phy_addr < phy_limit());
  if (isDRAM(phy_addr)) {
    uint64_t mach_addr = block_table_.LoadAddr(phy_addr);
    if (mach_addr == phy_addr) { // not cross addr
      mem_store_->OnDRAMRead(phy_addr);
      // suppose the page table is pre-modified
      mach_addr = page_table_.LoadAddr(phy_addr);
    } // else suppose the version buffer is in DRAM: no additional latency
    return mach_addr;
  } else {
    uint64_t mach_addr = block_table_.LoadAddr(phy_addr);
    mem_store_->OnNVMRead(mach_addr);
    return mach_addr;
  }
}

uint64_t AddrTransController::StoreDRAMAddr(uint64_t phy_addr) {
  if (page_table_.Probe(phy_addr) == EPOCH) {
    NewEpoch();
  }
  mem_store_->OnDRAMWrite(phy_addr);

  // pre-modification that should happen in the end of epoch
  uint64_t mach_addr = page_table_.StoreAddr(phy_addr);
  return mach_addr; 
}

uint64_t AddrTransController::StoreNVMAddr(uint64_t phy_addr, bool no_epoch) {
  if (block_table_.Probe(phy_addr) == EPOCH) {
    assert(!no_epoch);
    NewEpoch();
  }
  uint64_t mach_addr = block_table_.StoreAddr(phy_addr);
  mem_store_->OnNVMWrite(mach_addr);
  return mach_addr;
} 

uint64_t AddrTransController::StoreAddr(uint64_t phy_addr, bool cross) {
  assert(phy_addr < phy_limit());
  if (isDRAM(phy_addr)) {
    if (cross) {
      if (block_table_.Probe(phy_addr) == EPOCH) return INVAL_ADDR;
      cross_list_.push_back(phy_addr);
      return StoreNVMAddr(phy_addr, true);
    }
    return StoreDRAMAddr(phy_addr);
  } else {
    assert(!cross);
    return StoreNVMAddr(phy_addr);
  }
}

AddrStatus AddrTransController::Probe(uint64_t phy_addr, bool cross) {
  assert(phy_addr < phy_limit());
  AddrStatus status;
  status.type = isDRAM(phy_addr) ? DRAM_ADDR : NVM_ADDR;
  if (status.type) { // DRAM_ADDR
    if (cross) {
      status.oper = block_table_.Probe(phy_addr);
      if (status.oper == EPOCH) status.type = RETRY_REQ;
    } else {
      status.oper = page_table_.Probe(phy_addr);
    }
  } else {
    assert(!cross);
    status.oper = block_table_.Probe(phy_addr);
  }
  return status;
}

void AddrTransController::NewEpoch() {
  mem_store_->OnEpochEnd();
  page_table_.NewEpoch();
  block_table_.NewEpoch();
  while (!cross_list_.empty()) {
    uint64_t phy_addr = cross_list_.back();
    assert(isDRAM(phy_addr));
    block_table_.RevokeTag(block_table_.Tag(phy_addr));
    cross_list_.pop_back();
  }
  // Migration goes after this should it exist.
}

