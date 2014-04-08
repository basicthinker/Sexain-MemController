// addr_trans_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "addr_trans_controller.h"

uint64_t AddrTransController::LoadAddr(uint64_t phy_addr) {
  assert(phy_addr < phy_limit());
  uint64_t mach_addr = block_table_.LoadAddr(phy_addr);
  if (isDRAM(phy_addr)) {
    if (mach_addr == phy_addr) { // not cross addr
      mach_addr = page_table_.LoadAddr(phy_addr);
    } // else suppose the version buffer is in DRAM: no additional latency
    mem_store_->OnDRAMRead(phy_addr);
  } else {
    mem_store_->OnNVMRead(mach_addr);
  }
  return mach_addr;
}

uint64_t AddrTransController::StoreDRAMAddr(uint64_t phy_addr) {
  // pre-modification that should happen in the end of epoch
  uint64_t mach_addr = page_table_.StoreAddr(phy_addr);
  mem_store_->OnDRAMWrite(phy_addr); // as in-place update
  return mach_addr; 
}

uint64_t AddrTransController::StoreNVMAddr(uint64_t phy_addr) {
  uint64_t mach_addr = block_table_.StoreAddr(phy_addr);
  mem_store_->OnNVMWrite(mach_addr);
  return mach_addr;
} 

uint64_t AddrTransController::StoreAddr(uint64_t phy_addr, bool frozen) {
  assert(phy_addr < phy_limit());
  if (isDRAM(phy_addr)) {
    bool cross = isCross(phy_addr);
    if (!frozen && !cross) {
      return StoreDRAMAddr(phy_addr);
    }
    cross_list_.push_back(phy_addr);
  }
  return StoreNVMAddr(phy_addr);
}

AddrStatus AddrTransController::Probe(uint64_t phy_addr, bool frozen) {
  assert(phy_addr < phy_limit());
  AddrStatus status;
  status.type = isDRAM(phy_addr) ? DRAM_ADDR : NVM_ADDR;
  if (status.type == DRAM_ADDR && !frozen && !isCross(phy_addr)) {
    status.oper = page_table_.Probe(phy_addr);
  } else {
    status.oper = block_table_.Probe(phy_addr);
    if (frozen && status.oper == EPOCH) status.type = RETRY_REQ;
  }
  return status;
}

void AddrTransController::NewEpoch() {
  mem_store_->OnEpochEnd();
  page_table_.NewEpoch();
  block_table_.NewEpoch();
  for (std::vector<uint64_t>::iterator it = cross_list_.begin();
      it != cross_list_.end(); ++it) {
    assert(isDRAM(*it));
    block_table_.RevokeTag(block_table_.Tag(*it));
  }
  cross_list_.clear();
  // Migration goes after this should it exist.
}

