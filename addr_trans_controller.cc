// addr_trans_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "addr_trans_controller.h"

using namespace std;

#define FLAG_CROSS (0x1)

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
  int index;
  uint64_t phy_tag = att->Tag(phy_addr);
  uint64_t mach_addr = att->Lookup(phy_tag, &index);
  if (index == -EINVAL) {
    assert(att->GetLength(DIRTY_ENTRY) < att->length());
    mach_addr = vb->NewBlock();
    if (!att->IsEmpty(FREE_ENTRY)) {
      att->Setup(phy_tag, mach_addr, DIRTY_ENTRY);
    } else {
      pair<uint64_t, uint64_t> mapping = att->Replace(
          phy_tag, mach_addr, DIRTY_ENTRY);
      ms->NVMMove(att->Addr(mapping.first), mapping.second,
          att->block_size());
      vb->PinBlock(mapping.second);
    }
  } else if (att->At(index).state == CLEAN_ENTRY) {
    att->Revoke(phy_tag);
    vb->PinBlock(mach_addr);
    return phy_addr;
  } 
  return att->Translate(phy_addr, mach_addr);
}

bool AddrTransController::ProbeNVMStore(uint64_t phy_addr,
    AddrTransTable* att, VersionBuffer* vb, MemStore* ms) {
  uint64_t phy_tag = att->Tag(phy_addr);
  int index;
  att->Lookup(phy_tag, &index);
  return index != -EINVAL ||
      att->GetLength(DIRTY_ENTRY) != att->length();
}

uint64_t AddrTransController::DRAMStore(uint64_t phy_addr) {
  int index;
  uint64_t phy_tag = att_.Tag(phy_addr);
  uint64_t mach_addr = att_.Lookup(phy_tag, &index);
  if (index == -EINVAL) { // mapping exists
    if (in_ending_) return att_.Translate(phy_addr, mach_addr);
    else {
      assert(att_.At(index).state == TEMP_ENTRY);
      att_.Revoke(phy_tag);
      dram_buffer_.FreeBlock(mach_addr, IN_USE_SLOT);
      return phy_addr;
    }
  } else if (!in_ending_) {
    return phy_addr;
  } else { // in_ending
    assert(att_.GetLength(DIRTY_ENTRY) + att_.GetLength(TEMP_ENTRY)
        < att_.length());
    mach_addr = dram_buffer_.NewBlock();
    if (!att_.IsEmpty(FREE_ENTRY)) {
      att_.Setup(phy_tag, mach_addr, TEMP_ENTRY);
    } else {
      pair<uint64_t, uint64_t> mapping = att_.Replace(
          phy_tag, mach_addr, TEMP_ENTRY);
      mem_store_->NVMSwap(att_.Addr(mapping.first), mapping.second,
          att_.block_size());
      dram_buffer_.PinBlock(mapping.second);
      // suppose the mapping is marked in NV-ATT
    }
    return att_.Translate(phy_addr, mach_addr);
  }
}

uint64_t AddrTransController::StoreAddr(uint64_t phy_addr) {
  assert(phy_addr < phy_limit());
  if (isDRAM(phy_addr)) {
    return DRAMStore(phy_addr);
  } else {
    return NVMStore(phy_addr, &att_, &nvm_buffer_, mem_store_);
  }
}

bool AddrTransController::Probe(uint64_t phy_addr) {
  assert(phy_addr < phy_limit());
  if (isDRAM(phy_addr)) {
    if (!in_ending_) {
      return ProbeNVMStore(phy_addr, &ptt_, &ptt_buffer_, mem_store_); 
    } else {
      return ProbeNVMStore(phy_addr, &att_, &nvm_buffer_, mem_store_);
    }
  } else {
    return ProbeNVMStore(phy_addr, &att_, &nvm_buffer_, mem_store_);
  }
}

void AddrTransController::BeginEpochEnding() {
  assert(!in_ending_);
  TempEntryRevoker revoker(this);
  att_.VisitQueue(TEMP_ENTRY, &revoker); 
  att_.CleanDirtyQueue(); // suppose it is written back to NVM
  in_ending_ = true;
}

void AddrTransController::FinishEpochEnding() {
  assert(in_ending_);
  nvm_buffer_.FreeBackup(); 
  in_ending_ = false;
}

void AddrTransController::RevokeTempEntry(int index) {
  const ATTEntry& entry = att_.At(index);
  uint64_t phy_addr = att_.Addr(entry.phy_tag);
  if (entry.Test(ATTEntry::PHY_DRAM)) {
    assert(isDRAM(phy_addr));
    mem_store_->DRAMMove(phy_addr, entry.mach_addr, att_.block_size());
    dram_buffer_.FreeBlock(entry.mach_addr, IN_USE_SLOT);
    att_.FreeEntry(index);
  } else if (entry.Test(ATTEntry::MACH_RESET)) {
    mem_store_->WriteBack(phy_addr, entry.mach_addr, att_.block_size());
    dram_buffer_.FreeBlock(entry.mach_addr, IN_USE_SLOT);
    att_.FreeEntry(index);
  } else {
    uint64_t mach_addr = nvm_buffer_.NewBlock();
    mem_store_->WriteBack(phy_addr, entry.mach_addr, att_.block_size());
    att_.Reset(index, mach_addr, DIRTY_ENTRY);
  }
}

