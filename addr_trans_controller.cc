// addr_trans_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "addr_trans_controller.h"

using namespace std;

uint64_t AddrTransController::LoadAddr(uint64_t phy_addr) {
  assert(phy_addr < phy_limit());
  uint64_t phy_tag = att_.Tag(phy_addr);
  uint64_t final_addr = att_.Translate(phy_addr, att_.Lookup(phy_tag, NULL));
  if (isDRAM(phy_addr)) {
    mem_store_->OnDRAMRead(final_addr, att_.block_size());
  } else {
    mem_store_->OnNVMRead(final_addr, att_.block_size());
  }
  return final_addr;
}

uint64_t AddrTransController::NVMStore(uint64_t phy_addr,
    AddrTransTable* att, VersionBuffer* vb, MemStore* ms) {
  ATTState state;
  uint64_t phy_tag = att->Tag(phy_addr);
  uint64_t mach_addr = att->Lookup(phy_tag, &state);
  if (state == CLEAN_ENTRY) {
    att->Revoke(phy_tag);
    vb->PinBlock(mach_addr);
    return phy_addr;
  } else if (state == FREE_ENTRY) {
    if (att->GetLength(DIRTY_ENTRY) == att->length())
      return INVAL_ADDR; // indicates new epoch

    mach_addr = vb->NewBlock();
    if (!att->IsEmpty(FREE_ENTRY)) {
      att->Setup(phy_tag, mach_addr);
    } else {
      pair<uint64_t, uint64_t> mapping = att->Replace(phy_tag, mach_addr);
      ms->OnNVMMove(att->Addr(mapping.first), mapping.second,
          att->block_size());
      vb->PinBlock(mapping.second);
    }
  }
  return att->Translate(phy_addr, mach_addr);
}

uint64_t AddrTransController::DRAMStore(uint64_t phy_addr, bool frozen,
    AddrTransTable* att, VersionBuffer* vb, MemStore* ms) {
  return phy_addr; // TODO
}

uint64_t AddrTransController::StoreAddr(uint64_t phy_addr, bool frozen) {
  assert(phy_addr < phy_limit());
  if (isDRAM(phy_addr)) {
    return DRAMStore(phy_addr, frozen, &att_, &tmp_buffer_, mem_store_);
  } else {
    return NVMStore(phy_addr, &att_, &att_buffer_, mem_store_);
  }
}

void AddrTransController::NewEpoch() {
  att_.Clean();
  att_buffer_.CleanBackup();
  // Migration goes after this should it exist.
}

