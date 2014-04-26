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

uint64_t AddrTransController::NVMStore(uint64_t phy_addr) {
  const uint64_t phy_tag = att_.Tag(phy_addr);
  int index;
  const uint64_t mach_base = att_.Lookup(phy_tag, &index);
  if (!in_ending()) {
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.At(index);
      if (entry.state == TEMP_ENTRY) {
        return att_.Translate(phy_addr, RevokeTempEntry(index));
      } else if (entry.state == DIRTY_ENTRY) {
        return att_.Translate(phy_addr, mach_base);
      } else {
        assert(entry.state == CLEAN_ENTRY);
        att_.FreeEntry(index);
        nvm_buffer_.PinBlock(mach_base);
        return phy_addr;
      }
    } else {
      assert(att_.GetLength(DIRTY_ENTRY) < att_.length());
      const uint64_t new_base = nvm_buffer_.NewBlock();
      if (!att_.IsEmpty(FREE_ENTRY)) {
        att_.Setup(phy_tag, new_base, DIRTY_ENTRY, ATTEntry::NON_TEMP);
      } else if (!att_.IsEmpty(TEMP_ENTRY)) {
        int tmp_i = att_.GetFront(TEMP_ENTRY);
        RevokeTempEntry(tmp_i);
        att_.Setup(phy_tag, new_base, DIRTY_ENTRY, ATTEntry::NON_TEMP);
      } else {
        pair<uint64_t, uint64_t> replaced = att_.Replace(
            phy_tag, new_base, DIRTY_ENTRY, ATTEntry::NON_TEMP);
        mem_store_->NVMMove(att_.Addr(replaced.first), replaced.second,
            att_.block_size());
        nvm_buffer_.PinBlock(replaced.second);
      }
      return att_.Translate(phy_addr, new_base);
    }
  } else { // in ending
    return -EINVAL;
  }
}

uint64_t AddrTransController::DRAMStore(uint64_t phy_addr) {
  int index;
  uint64_t phy_tag = att_.Tag(phy_addr);
  uint64_t mach_base = att_.Lookup(phy_tag, &index);
  if (index != -EINVAL) { // mapping exists
    if (in_ending()) return att_.Translate(phy_addr, mach_base);
    else {
      assert(att_.At(index).state == TEMP_ENTRY);
      att_.FreeEntry(index);
      dram_buffer_.FreeBlock(mach_base, IN_USE_SLOT);
      return phy_addr;
    }
  } else if (!in_ending()) {
    return phy_addr;
  } else { // in ending
    assert(att_.GetLength(DIRTY_ENTRY) + att_.GetLength(TEMP_ENTRY)
        < att_.length());
    mach_base = dram_buffer_.NewBlock();
    if (!att_.IsEmpty(FREE_ENTRY)) {
      att_.Setup(phy_tag, mach_base, TEMP_ENTRY, ATTEntry::PHY_DRAM);
    } else {
      pair<uint64_t, uint64_t> mapping = att_.Replace(
          phy_tag, mach_base, TEMP_ENTRY, ATTEntry::PHY_DRAM);
      mem_store_->NVMSwap(att_.Addr(mapping.first), mapping.second,
          att_.block_size());
      nvm_buffer_.PinBlock(mapping.second);
      // suppose the mapping is marked in NV-ATT
    }
    return att_.Translate(phy_addr, mach_base);
  }
}

void AddrTransController::PseudoPageStore(uint64_t phy_addr) {
  int index = -EINVAL;
  uint64_t phy_tag = ptt_.Tag(phy_addr);
  uint64_t mach_base = ptt_.Lookup(phy_tag, &index);
  if (index != -EINVAL) {
    if (ptt_.At(index).state != DIRTY_ENTRY) {
      assert(ptt_.At(index).state == CLEAN_ENTRY);
      ptt_.FreeEntry(index);
      ptt_buffer_.PinBlock(mach_base);
    }
  } else {
    assert(ptt_.GetLength(DIRTY_ENTRY) < ptt_.length());
    mach_base = ptt_buffer_.NewBlock();
    if (!ptt_.IsEmpty(FREE_ENTRY)) {
      ptt_.Setup(phy_tag, mach_base, DIRTY_ENTRY, ATTEntry::NON_TEMP);
    } else {
      pair<uint64_t, uint64_t> mapping =
          ptt_.Replace(phy_tag, mach_base, DIRTY_ENTRY, ATTEntry::NON_TEMP);
      ++num_page_move_;
      ptt_buffer_.PinBlock(mapping.second);
    }
  }
}

uint64_t AddrTransController::StoreAddr(uint64_t phy_addr) {
  assert(phy_addr < phy_limit());
  if (isDRAM(phy_addr)) {
    PseudoPageStore(phy_addr);
    return DRAMStore(phy_addr);
  } else {
    return NVMStore(phy_addr);
  }
}

ATTControl AddrTransController::Probe(uint64_t phy_addr) {
  if (isDRAM(phy_addr)) {
    bool ptt_avail = ptt_.Contains(phy_addr) || !ptt_.IsFull();
    if (in_ending()) {
      bool att_avail = att_.Contains(phy_addr) || !att_.IsFull();
      if (!att_avail || !ptt_avail) {
        return ATC_RETRY;
      }
    } else if (!ptt_avail) {
      return ATC_EPOCH;
    }
  }
  return ATC_ACCEPT;
}

void AddrTransController::BeginEpochEnding() {
  assert(!in_ending());
  TempEntryRevoker revoker(this);
  att_.VisitQueue(TEMP_ENTRY, &revoker);
  assert(att_.IsEmpty(TEMP_ENTRY)); 
  att_.CleanDirtyQueue(); // suppose it is written back to NVM
  in_ending_ = true;

  ptt_.CleanDirtyQueue(); // begin to deal with the next epoch
  ptt_buffer_.FreeBackup(); // without non-stall property 
  num_page_move_ = 0;
}

void AddrTransController::FinishEpochEnding() {
  assert(in_ending());
  nvm_buffer_.FreeBackup();
  in_ending_ = false;
}

uint64_t AddrTransController::RevokeTempEntry(int index) {
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == TEMP_ENTRY);
  uint64_t phy_base = att_.Addr(entry.phy_tag);
  if (entry.Test(ATTEntry::PHY_DRAM)) {
    assert(isDRAM(phy_base));
    mem_store_->DRAMMove(phy_base, entry.mach_base, att_.block_size());
    dram_buffer_.FreeBlock(entry.mach_base, IN_USE_SLOT);
    att_.FreeEntry(index);
    return phy_base;
  } else if (entry.Test(ATTEntry::MACH_RESET)) {
    mem_store_->WriteBack(phy_base, entry.mach_base, att_.block_size());
    dram_buffer_.FreeBlock(entry.mach_base, IN_USE_SLOT);
    att_.FreeEntry(index);
    return phy_base;
  } else {
    uint64_t mach_base = nvm_buffer_.NewBlock();
    mem_store_->WriteBack(mach_base, entry.mach_base, att_.block_size());
    dram_buffer_.FreeBlock(entry.mach_base, IN_USE_SLOT);
    att_.Reset(index, mach_base, DIRTY_ENTRY, ATTEntry::NON_TEMP);
    return mach_base;
  }
}

