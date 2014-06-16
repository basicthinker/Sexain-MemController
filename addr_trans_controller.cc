// addr_trans_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "addr_trans_controller.h"

using namespace std;

Addr AddrTransController::LoadAddr(Addr phy_addr) {
  assert(phy_addr < PhyLimit());
  Tag phy_tag = att_.ToTag(phy_addr);
  Addr mach_addr = att_.Translate(phy_addr, att_.Lookup(phy_tag).second);
  if (IsDRAM(phy_addr)) {
    mem_store_->OnDRAMRead(mach_addr, att_.block_size());
  } else {
    mem_store_->OnNVMRead(mach_addr, att_.block_size());
  }
  return mach_addr;
}

Addr AddrTransController::NVMStore(Addr phy_addr, int size) {
  /*const Tag phy_tag = att_.ToTag(phy_addr);
  int index = att_.Lookup(phy_tag).first;
  if (!in_checkpointing()) {
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.At(index);
      if (entry.IsCrossTemp()) {
        ATTRevokeTemp(index, !FullBlock(phy_addr, size));
        return phy_addr;
      } else if (entry.IsCrossDirty()) {
        Addr mach_base = ATTRegularizeDirty(index, !FullBlock(phy_addr, size));
        return att_.Translate(phy_addr, mach_base);
      } else if (entry.state == ATTEntry::DIRTY) { // regular dirty
        return att_.Translate(phy_addr, entry.mach_base);
      } else {
        nvm_buffer_.PinBlock(entry.mach_base);
        ATTShrink(index, !FullBlock(phy_addr, size));
        return phy_addr;
      }
    } else { // not found
      assert(att_.GetLength(ATTEntry::DIRTY) < att_.length());
      const Addr mach_base = nvm_buffer_.NewBlock();
      if (att_.IsEmpty(ATTEntry::FREE)) {
        if (att_.IsEmpty(ATTEntry::TEMP)) {
          int ci = att_.GetFront(ATTEntry::CLEAN);
          nvm_buffer_.PinBlock(att_.At(ci).mach_base);
          ATTShrink(ci);
        } else {
          int ti = att_.GetFront(ATTEntry::TEMP);
          ATTRevokeTemp(ti);
        }
      }
      ATTSetup(phy_tag, mach_base, ATTEntry::DIRTY, ATTEntry::REGULAR,
          !FullBlock(phy_addr, size));
      return att_.Translate(phy_addr, mach_base);
    }
  } else { // while checkpointing
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.At(index);
      if (entry.IsRegularDirty() || entry.state == ATTEntry::TEMP) {
        return att_.Translate(phy_addr, entry.mach_base);
      } else {
        const Addr mach_base = dram_buffer_.NewBlock();
        nvm_buffer_.PinBlock(entry.mach_base);
        ATTReset(index, mach_base, !FullBlock(phy_addr, size));
        return att_.Translate(phy_addr, mach_base);
      }
    } else { // not found
      assert(att_.GetLength({ATTEntry::DIRTY, ATTEntry::TEMP}) < att_.length());
      if (!att_.IsEmpty(ATTEntry::FREE)) {
        const Addr mach_base = nvm_buffer_.NewBlock();
        ATTSetup(phy_tag, mach_base, ATTEntry::DIRTY, ATTEntry::REGULAR,
            !FullBlock(phy_addr, size));
        return att_.Translate(phy_addr, mach_base);
      } else {
        const Addr mach_base = dram_buffer_.NewBlock();
        int ci = att_.GetFront(ATTEntry::CLEAN);
        const ATTEntry& entry = att_.At(ci);
        mem_store_->Swap(att_.ToAddr(entry.phy_tag), entry.mach_base,
            att_.block_size());
        nvm_buffer_.PinBlock(entry.mach_base);
        ATTShrink(ci, false);
        // suppose the NV-ATT has the replaced marked as swapped
        ATTSetup(phy_addr, mach_base, ATTEntry::DIRTY, ATTEntry::CROSS,
            !FullBlock(phy_addr, size));
        return att_.Translate(phy_addr, mach_base);
      }
    }
  } */
  return phy_addr;
}

Addr AddrTransController::DRAMStore(Addr phy_addr, int size) {
  int index = att_.Lookup(att_.ToTag(phy_addr)).first;
  if (index != -EINVAL) { // found
    const ATTEntry& entry = att_.At(index);
    if (in_checkpointing()) {
      return att_.Translate(phy_addr, entry.mach_base);
    } else {
      FreeRegTemp(index, !FullBlock(phy_addr, size));
      return phy_addr;
    }
  } else { // not found
    if (in_checkpointing()) {
      if (att_.IsEmpty(ATTEntry::FREE)) {
        assert(!att_.IsEmpty(ATTEntry::VISIBLE_CLEAN));
        int ci = att_.GetFront(ATTEntry::VISIBLE_CLEAN);
        FreeVisibleClean(ci, !FullBlock(phy_addr, size));
        // suppose the mapping is marked in NV-ATT
      }
      const Addr mach_base = dram_buffer_.NewBlock();
      Setup(phy_addr, mach_base, size, ATTEntry::REG_TEMP);
      return att_.Translate(phy_addr, mach_base);
    } else {
      return phy_addr;
    }
  }
}

void AddrTransController::PseudoPageStore(Tag phy_tag) {
  const pair<int, Addr> target = ptt_.Lookup(phy_tag);

  if (target.first != -EINVAL) { // found
    const ATTEntry &entry = ptt_.At(target.first);
    if (entry.state == ATTEntry::REG_DIRTY ||
        entry.state == ATTEntry::HIDDEN_CLEAN) {
      return;
    } else {
      assert(entry.state == ATTEntry::VISIBLE_CLEAN);
      ptt_buffer_.PinBlock(target.second);
      ptt_.ShiftState(target.first, ATTEntry::HIDDEN_CLEAN);
    }
  } else {
    if (ptt_.IsEmpty(ATTEntry::FREE)) {
      assert(!ptt_.IsEmpty(ATTEntry::VISIBLE_CLEAN));
      int ci = ptt_.GetFront(ATTEntry::VISIBLE_CLEAN);
      const ATTEntry& entry = ptt_.At(ci);

      ++pages_twice_written_;
      ptt_buffer_.PinBlock(entry.mach_base);
      ptt_.Revoke(ci);
    }

    Addr mach_base = ptt_buffer_.NewBlock();
    ptt_.Setup(phy_tag, mach_base, ATTEntry::REG_DIRTY);
  }
}

Addr AddrTransController::StoreAddr(Addr phy_addr, int size) {
  assert(CheckValid(phy_addr, size) && phy_addr < PhyLimit());
  if (IsDRAM(phy_addr)) {
    PseudoPageStore(ptt_.ToTag(phy_addr));
    return DRAMStore(phy_addr, size);
  } else {
    return NVMStore(phy_addr, size);
  }
}

ATTControl AddrTransController::Probe(Addr phy_addr) {

  if (IsDRAM(phy_addr)) {
    bool ptt_avail = ptt_.Contains(phy_addr) ||
        ptt_.GetLength(ATTEntry::REG_DIRTY) < ptt_.length();
    if (in_checkpointing()) {
      bool att_avail = att_.Contains(phy_addr) ||
          att_.GetLength(ATTEntry::REG_DIRTY) < att_.length();
      if (!att_avail || !ptt_avail) {
        return ATC_RETRY;
      }
    } else if (!ptt_avail) {
      return ATC_EPOCH;
    }
  }/* else { // NVM
    if (in_checkpointing() && !att_.Contains(phy_addr) &&
        att_.GetLength({ATTEntry::DIRTY, ATTEntry::TEMP}) == att_.length()) {
      return ATC_RETRY;
    } else if (!in_checkpointing() && !att_.Contains(phy_addr) &&
        att_.GetLength(ATTEntry::DIRTY) == att_.length()) {
      return ATC_EPOCH;
    }
  }*/
  return ATC_ACCEPT;
}

void AddrTransController::BeginEpochEnding() {
/*  assert(!in_checkpointing());
  TempEntryRevoker temp_revoker(*this);
  att_.VisitQueue(ATTEntry::TEMP, &temp_revoker);
  assert(att_.IsEmpty(ATTEntry::TEMP)); 

  DirtyEntryRevoker dirty_revoker(*this);
  att_.VisitQueue(ATTEntry::DIRTY, &dirty_revoker);
  // suppose regular dirty entries are written back to NVM
  DirtyEntryCleaner att_cleaner(att_);
  att_.VisitQueue(ATTEntry::DIRTY, &att_cleaner);
  assert(att_.IsEmpty(ATTEntry::DIRTY));

  in_checkpointing_ = true;

  // begin to deal with the next epoch
  DirtyEntryCleaner ptt_cleaner(ptt_);
  ptt_.VisitQueue(ATTEntry::DIRTY, &ptt_cleaner);
  assert(ptt_.IsEmpty(ATTEntry::DIRTY));

  ptt_buffer_.FreeBackup(); // without non-stall property 
  num_page_move_ = 0;*/
}

void AddrTransController::FinishEpochEnding() {
  assert(in_checkpointing());
  nvm_buffer_.FreeBackup();
  in_checkpointing_ = false;
}

void AddrTransController::Setup(Addr phy_addr, Addr mach_base, int size,
    ATTEntry::State state) {
  assert(state == ATTEntry::REG_DIRTY || state == ATTEntry::CROSS_DIRTY ||
      state == ATTEntry::REG_TEMP);

  const Tag phy_tag = att_.ToTag(phy_addr);
  if (!FullBlock(phy_addr, size)) {
    mem_store_->Move(mach_base, phy_addr, att_.block_size());
  }
  att_.Setup(phy_tag, mach_base, state);
}

void AddrTransController::HideClean(int index, bool move_data) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::VISIBLE_CLEAN);

  const Addr phy_addr = att_.ToAddr(entry.phy_tag);
  if (move_data) {
    mem_store_->Move(phy_addr, entry.mach_base, att_.block_size());
  }
  nvm_buffer_.PinBlock(entry.mach_base);
  att_.ShiftState(index, ATTEntry::HIDDEN_CLEAN);
}

Addr AddrTransController::ResetVisibleClean(int index, bool move_data) {
  assert(in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::VISIBLE_CLEAN);

  const Addr mach_base = dram_buffer_.NewBlock();
  if (move_data) {
    mem_store_->Move(mach_base, entry.mach_base, att_.block_size());
  }
  nvm_buffer_.PinBlock(entry.mach_base);
  att_.Reset(index, mach_base, ATTEntry::CROSS_TEMP);
  return mach_base;
}

void AddrTransController::FreeVisibleClean(int index, bool move_data) {
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::VISIBLE_CLEAN);

  Addr phy_addr = att_.ToAddr(entry.phy_tag);
  if (in_checkpointing()) {
    mem_store_->Swap(phy_addr, entry.mach_base, att_.block_size());
  } else {
    mem_store_->Move(phy_addr, entry.mach_base, att_.block_size());
  }
  nvm_buffer_.PinBlock(entry.mach_base);
  att_.Revoke(index);
}

Addr AddrTransController::RegularizeDirty(int index, bool move_data) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::CROSS_DIRTY);

  const Addr mach_base = nvm_buffer_.NewBlock();
  if (move_data) {
    mem_store_->Move(mach_base, entry.mach_base, att_.block_size());
  }
  dram_buffer_.FreeBlock(entry.mach_base, VersionBuffer::IN_USE);
  att_.Reset(index, mach_base, ATTEntry::REG_DIRTY);
  return mach_base;
}

void AddrTransController::FreeRegTemp(int index, bool move_data) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::REG_TEMP);

  const Addr phy_addr = att_.ToAddr(entry.phy_tag);
  if (move_data) {
    mem_store_->Move(phy_addr, entry.mach_base, att_.block_size());
  }
  dram_buffer_.FreeBlock(entry.mach_base, VersionBuffer::IN_USE);
  att_.Revoke(index);
}

void AddrTransController::CleanCrossTemp(int index, bool move_data) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::CROSS_TEMP);

  const Addr phy_addr = att_.ToAddr(entry.phy_tag);
  if (move_data) {
    mem_store_->Move(phy_addr, entry.mach_base, att_.block_size());
  }
  dram_buffer_.FreeBlock(entry.mach_base, VersionBuffer::IN_USE);
  att_.ShiftState(index, ATTEntry::HIDDEN_CLEAN);
}
