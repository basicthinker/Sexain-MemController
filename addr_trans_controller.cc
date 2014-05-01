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

uint64_t AddrTransController::NVMStore(uint64_t phy_addr) {
  const uint64_t phy_tag = att_.Tag(phy_addr);
  int index;
  att_.Lookup(phy_tag, &index);
  if (!in_ending()) {
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.At(index);
      if (entry.IsPlaceholder() || entry.IsReset()) {
        return att_.Translate(phy_addr, RevokeEntry(index, true));
      } else if (entry.IsRegularDirty()) {
        return att_.Translate(phy_addr, entry.mach_base);
      } else {
        assert(entry.state == ATTEntry::CLEAN);
        nvm_buffer_.PinBlock(entry.mach_base);
        att_.FreeEntry(index);
        return phy_addr;
      }
    } else { // not found
      assert(att_.GetLength(ATTEntry::DIRTY) < att_.length());
      const uint64_t mach_base = nvm_buffer_.NewBlock();
      if (!att_.IsEmpty(ATTEntry::FREE)) {
        att_.Setup(phy_tag, mach_base, ATTEntry::DIRTY, ATTEntry::REGULAR);
      } else if (!att_.IsEmpty(ATTEntry::TEMP)) {
        int temp_i = att_.GetFront(ATTEntry::TEMP);
        RevokeEntry(temp_i, false);
        att_.Setup(phy_tag, mach_base, ATTEntry::DIRTY, ATTEntry::REGULAR);
      } else {
        pair<uint64_t, uint64_t> replaced = att_.Replace(
            phy_tag, mach_base, ATTEntry::DIRTY, ATTEntry::REGULAR);
        mem_store_->NVMMove(att_.Addr(replaced.first), replaced.second,
            att_.block_size());
        nvm_buffer_.PinBlock(replaced.second);
      }
      return att_.Translate(phy_addr, mach_base);
    }
  } else { // in ending
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.At(index);
      if (entry.IsRegularDirty() || entry.state == ATTEntry::TEMP) {
        return att_.Translate(phy_addr, entry.mach_base);
      } else {
        const uint64_t mach_base = dram_buffer_.NewBlock();
        nvm_buffer_.PinBlock(entry.mach_base);
        att_.Reset(index, mach_base, ATTEntry::TEMP, ATTEntry::CROSS);
        return att_.Translate(phy_addr, mach_base);
      }
    } else { // not found
      assert(att_.GetLength({ATTEntry::DIRTY, ATTEntry::TEMP}) < att_.length());
      if (!att_.IsEmpty(ATTEntry::FREE)) {
        const uint64_t mach_base = nvm_buffer_.NewBlock();
        att_.Setup(phy_tag, mach_base, ATTEntry::DIRTY, ATTEntry::REGULAR);
        return att_.Translate(phy_addr, mach_base);
      } else {
        const uint64_t mach_base = dram_buffer_.NewBlock();
        pair<uint64_t, uint64_t> replaced = att_.Replace(
            phy_addr, mach_base, ATTEntry::DIRTY, ATTEntry::CROSS);
        mem_store_->NVMSwap(att_.Addr(replaced.first), replaced.second,
            att_.block_size());
        // suppose the NV-ATT has the replaced marked as swapped
        nvm_buffer_.PinBlock(replaced.second);
        return att_.Translate(phy_addr, mach_base);
      }
    }
  }
}

uint64_t AddrTransController::DRAMStore(uint64_t phy_addr) {
  int index;
  uint64_t phy_tag = att_.Tag(phy_addr);
  att_.Lookup(phy_tag, &index);
  if (index != -EINVAL) { // found
    const ATTEntry& entry = att_.At(index);
    if (in_ending()) return att_.Translate(phy_addr, entry.mach_base);
    else {
      assert(entry.IsRegularTemp());
      return att_.Translate(phy_addr, RevokeEntry(index, true));
    }
  } else if (!in_ending()) {
    return phy_addr;
  } else { // in ending
    assert(att_.GetLength({ATTEntry::DIRTY, ATTEntry::TEMP}) < att_.length());
    const uint64_t mach_base = dram_buffer_.NewBlock();
    if (!att_.IsEmpty(ATTEntry::FREE)) {
      att_.Setup(phy_tag, mach_base, ATTEntry::TEMP, ATTEntry::REGULAR);
    } else {
      pair<uint64_t, uint64_t> mapping = att_.Replace(
          phy_tag, mach_base, ATTEntry::TEMP, ATTEntry::REGULAR);
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
    if (ptt_.At(index).state != ATTEntry::DIRTY) {
      assert(ptt_.At(index).state == ATTEntry::CLEAN);
      ptt_.FreeEntry(index);
      ptt_buffer_.PinBlock(mach_base);
    }
  } else {
    assert(ptt_.GetLength(ATTEntry::DIRTY) < ptt_.length());
    mach_base = ptt_buffer_.NewBlock();
    if (!ptt_.IsEmpty(ATTEntry::FREE)) {
      ptt_.Setup(phy_tag, mach_base, ATTEntry::DIRTY, ATTEntry::REGULAR);
    } else {
      pair<uint64_t, uint64_t> mapping =
          ptt_.Replace(phy_tag, mach_base, ATTEntry::DIRTY, ATTEntry::REGULAR);
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
    bool ptt_avail = ptt_.Contains(phy_addr) ||
        ptt_.GetLength(ATTEntry::DIRTY) < ptt_.length();
    if (in_ending()) {
      bool att_avail = att_.Contains(phy_addr) ||
          att_.GetLength({ATTEntry::DIRTY, ATTEntry::TEMP}) < att_.length();
      if (!att_avail || !ptt_avail) {
        return ATC_RETRY;
      }
    } else if (!ptt_avail) {
      return ATC_EPOCH;
    }
  } else { // NVM
    if (in_ending() && !att_.Contains(phy_addr) &&
        att_.GetLength({ATTEntry::DIRTY, ATTEntry::TEMP}) == att_.length()) {
      return ATC_RETRY;
    } else if (!in_ending() && !att_.Contains(phy_addr) &&
        att_.GetLength(ATTEntry::DIRTY) == att_.length()) {
      return ATC_EPOCH;
    }
  }
  return ATC_ACCEPT;
}

void AddrTransController::BeginEpochEnding() {
  assert(!in_ending());
  TempEntryRevoker temp_revoker(*this);
  att_.VisitQueue(ATTEntry::TEMP, &temp_revoker);
  assert(att_.IsEmpty(ATTEntry::TEMP)); 

  DirtyEntryRevoker dirty_revoker(*this);
  att_.VisitQueue(ATTEntry::DIRTY, &dirty_revoker);
  // suppose regular dirty entries are written back to NVM
  DirtyEntryCleaner att_cleaner(att_);
  att_.VisitQueue(ATTEntry::DIRTY, &att_cleaner);
  assert(att_.IsEmpty(ATTEntry::DIRTY));

  in_ending_ = true;

  // begin to deal with the next epoch
  DirtyEntryCleaner ptt_cleaner(ptt_);
  ptt_.VisitQueue(ATTEntry::DIRTY, &ptt_cleaner);
  assert(ptt_.IsEmpty(ATTEntry::DIRTY));

  ptt_buffer_.FreeBackup(); // without non-stall property 
  num_page_move_ = 0;
}

void AddrTransController::FinishEpochEnding() {
  assert(in_ending());
  nvm_buffer_.FreeBackup();
  in_ending_ = false;
}

uint64_t AddrTransController::RevokeEntry(int index, bool discard_data) {
  assert(!in_ending());
  const ATTEntry& entry = att_.At(index);
  uint64_t phy_base = att_.Addr(entry.phy_tag);
  if (entry.IsPlaceholder()) {
    uint64_t mach_base = nvm_buffer_.NewBlock();
    if (!discard_data) {
      mem_store_->WriteBack(mach_base, entry.mach_base, att_.block_size());
    }
    dram_buffer_.FreeBlock(entry.mach_base, IN_USE_SLOT);
    att_.Reset(index, mach_base, ATTEntry::DIRTY, ATTEntry::REGULAR);
    return mach_base;
  } else if (entry.IsReset()) {
    if (!discard_data) {
      mem_store_->WriteBack(phy_base, entry.mach_base, att_.block_size());
    }
    dram_buffer_.FreeBlock(entry.mach_base, IN_USE_SLOT);
    att_.FreeEntry(index);
    return phy_base;
  } else if (entry.IsRegularTemp()) {
    assert(isDRAM(phy_base));
    if (!discard_data) {
      mem_store_->DRAMMove(phy_base, entry.mach_base, att_.block_size());
    }
    dram_buffer_.FreeBlock(entry.mach_base, IN_USE_SLOT);
    att_.FreeEntry(index);
    return phy_base;
  } else assert(false);
}

