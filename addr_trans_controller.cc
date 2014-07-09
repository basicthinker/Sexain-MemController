// addr_trans_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "addr_trans_controller.h"

using namespace std;

Addr AddrTransController::LoadAddr(Addr phy_addr) {
  assert(phy_addr < PhyLimit());
  Tag phy_tag = att_.ToTag(phy_addr);
  Addr mach_addr = att_.Translate(phy_addr, ATTLookup(att_, phy_tag).second);
  if (IsStatic(phy_addr)) {
    mem_store_->OnDRAMRead(mach_addr, att_.block_size());
  } else {
    mem_store_->OnNVMRead(mach_addr, att_.block_size());
  }
  return mach_addr;
}

Addr AddrTransController::NVMStore(Addr phy_addr, int size) {
  const Tag phy_tag = att_.ToTag(phy_addr);
  int index = ATTLookup(att_, phy_tag).first;
  Addr mach_addr;
  if (!in_checkpointing()) {
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.At(index);
      switch(entry.state) {
      case ATTEntry::DIRTY:
        mach_addr = att_.Translate(phy_addr, entry.mach_base);
        break;
      case ATTEntry::TEMP:
        HideTemp(index, !FullBlock(phy_addr, size));
      case ATTEntry::HIDDEN:
        mach_addr = phy_addr;
        break;
      case ATTEntry::STAINED:
        mach_addr = att_.Translate(phy_addr,
            DirtyStained(index, !FullBlock(phy_addr, size)));
        break;
      default:
        HideClean(index, !FullBlock(phy_addr, size));
        mach_addr = phy_addr;
        break;
      }
    } else { // not found
      if (att_.IsEmpty(ATTEntry::FREE)) {
        if (!att_.IsEmpty(ATTEntry::LOAN)) {
          int li = ATTFront(att_, ATTEntry::LOAN);
          FreeLoan(li);
        } else if(!att_.IsEmpty(ATTEntry::CLEAN)) {
          int ci = ATTFront(att_, ATTEntry::CLEAN);
          FreeClean(ci);
        } else {
          BeginCheckpointing();
          return StoreAddr(phy_addr, size);
        }
      } else mem_store_->OnATTFreeSetup(phy_addr, ATTEntry::DIRTY);
      Addr mach_base = VBNewBlock(nvm_buffer_);
      Setup(phy_addr, mach_base, size, ATTEntry::DIRTY);
      mach_addr = att_.Translate(phy_addr, mach_base);
    }
    mem_store_->OnNVMWrite(mach_addr, size);
    return mach_addr;
  } else { // in checkpointing
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.At(index);
      if (entry.state == ATTEntry::TEMP || entry.state == ATTEntry::STAINED) {
        mach_addr = att_.Translate(phy_addr, entry.mach_base);
      } else {
        Addr mach_base = ResetClean(index, !FullBlock(phy_addr, size));
        mach_addr = att_.Translate(phy_addr, mach_base);
      }
    } else { // not found
      if (att_.IsEmpty(ATTEntry::FREE)) {
        if (!att_.IsEmpty(ATTEntry::CLEAN)) {
          int ci = ATTFront(att_, ATTEntry::CLEAN);
          FreeClean(ci);
        } else {
          mem_store_->OnWaiting();
          return INVAL_ADDR;
        }
      } else mem_store_->OnATTFreeSetup(phy_addr, ATTEntry::STAINED);
      Addr mach_base = dram_buffer_.NewBlock();
      Setup(phy_addr, mach_base, size, ATTEntry::STAINED);
      mach_addr = att_.Translate(phy_addr, mach_base);
    }
    mem_store_->OnDRAMWrite(mach_addr, size);
    return mach_addr;
  }
}

Addr AddrTransController::DRAMStore(Addr phy_addr, int size) {
  Addr mach_addr;
  int index = ATTLookup(att_, att_.ToTag(phy_addr)).first;
  if (index != -EINVAL) { // found
    const ATTEntry& entry = att_.At(index);
    if (in_checkpointing()) {
      mach_addr = att_.Translate(phy_addr, entry.mach_base);
    } else {
      FreeLoan(index, !FullBlock(phy_addr, size));
      mach_addr = phy_addr;
    }
  } else { // not found
    if (in_checkpointing()) {
      if (att_.IsEmpty(ATTEntry::FREE)) {
        if (!att_.IsEmpty(ATTEntry::CLEAN)) {
          int ci = ATTFront(att_, ATTEntry::CLEAN);
          FreeClean(ci);
        } else {
          mem_store_->OnWaiting();
          return INVAL_ADDR;
        }
      } else mem_store_->OnATTFreeSetup(phy_addr, ATTEntry::LOAN);
      const Addr mach_base = dram_buffer_.NewBlock();
      Setup(phy_addr, mach_base, size, ATTEntry::LOAN);
      mach_addr = att_.Translate(phy_addr, mach_base);
    } else { // in running
      mach_addr = phy_addr;
    }
  }
  mem_store_->OnDRAMWrite(mach_addr, size);
  return mach_addr;
}

bool AddrTransController::PseudoPageStore(Tag phy_tag) {
  const pair<int, Addr> target = ATTLookup(ptt_, phy_tag);

  if (target.first != -EINVAL) { // found
    const ATTEntry &entry = ptt_.At(target.first);
    if (entry.state != ATTEntry::DIRTY &&
        entry.state != ATTEntry::HIDDEN) {
      assert(entry.state == ATTEntry::CLEAN);
      ATTShiftState(ptt_, target.first, ATTEntry::HIDDEN);
    }
  } else {
    if (ptt_.IsEmpty(ATTEntry::FREE)) {
      if (!ptt_.IsEmpty(ATTEntry::CLEAN)) {
        int ci = ATTFront(ptt_, ATTEntry::CLEAN);
        ATTShiftState(ptt_, ci, ATTEntry::FREE);
        ++pages_twice_written_;
      } else if (in_checkpointing()) {
        mem_store_->OnWaiting();
        return false;
      } else {
        BeginCheckpointing();
        return true;
      }
    }
    ATTSetup(ptt_, phy_tag, INVAL_ADDR, ATTEntry::DIRTY);
  }
  return true;
}

Addr AddrTransController::StoreAddr(Addr phy_addr, int size) {
  assert(CheckValid(phy_addr, size) && phy_addr < PhyLimit());
  if (IsStatic(phy_addr)) {
    if (PseudoPageStore(ptt_.ToTag(phy_addr))) {
      return DRAMStore(phy_addr, size);
    } else return INVAL_ADDR;
  } else {
    return NVMStore(phy_addr, size);
  }
}

inline void AddrTransController::DirtyCleaner::Visit(int i) {
  const ATTEntry& entry = atc_->att_.At(i);
  if (entry.state == ATTEntry::STAINED) {
    atc_->DirtyStained(i);
  } else if (entry.state == ATTEntry::TEMP) {
    atc_->HideTemp(i);
  }

  if (entry.state == ATTEntry::DIRTY) {
    atc_->ATTShiftState(atc_->att_, i, ATTEntry::CLEAN);
    ++num_new_entries_;
  } else if (entry.state == ATTEntry::HIDDEN) {
    atc_->ATTShiftState(atc_->att_, i, ATTEntry::FREE);
  }
}

inline void AddrTransController::LoanRevoker::Visit(int i) {
  const ATTEntry& entry = atc_->att_.At(i);
  assert(entry.state == ATTEntry::LOAN);
  atc_->FreeLoan(i);
}

inline void AddrTransController::PTTCleaner::Visit(int i) {
  const ATTEntry& entry = atc_->ptt_.At(i);
  if (entry.state == ATTEntry::DIRTY) {
    atc_->ATTShiftState(atc_->ptt_, i, ATTEntry::CLEAN);
    ++num_new_entries_;
  } else {
    assert(entry.state == ATTEntry::HIDDEN);
    atc_->ATTShiftState(atc_->ptt_, i, ATTEntry::FREE);
  }
}

void AddrTransController::BeginCheckpointing() {
  assert(!in_checkpointing());

  DirtyCleaner att_cleaner(this);
  ATTVisit(att_, ATTEntry::DIRTY, &att_cleaner);
  assert(att_.IsEmpty(ATTEntry::DIRTY));

  LoanRevoker loan_revoker(this);
  ATTVisit(att_, ATTEntry::LOAN, &loan_revoker);
  assert(att_.IsEmpty(ATTEntry::LOAN));

  assert(att_.GetLength(ATTEntry::CLEAN) +
      att_.GetLength(ATTEntry::FREE) == att_.length());

  PTTCleaner ptt_cleaner(this);
  ATTVisit(ptt_, ATTEntry::DIRTY, &ptt_cleaner);
  assert(ptt_.IsEmpty(ATTEntry::DIRTY));

  in_checkpointing_ = true;
  mem_store_->OnCheckpointing(
      att_cleaner.num_new_entries(), ptt_cleaner.num_new_entries());
}

void AddrTransController::FinishCheckpointing() {
  assert(in_checkpointing());
  nvm_buffer_.ClearBackup();
  in_checkpointing_ = false;
  mem_store_->OnEpochEnd();
}

void AddrTransController::Setup(Addr phy_addr, Addr mach_base, int size,
    ATTEntry::State state) {
  assert(state == ATTEntry::DIRTY || state == ATTEntry::STAINED
      || state == ATTEntry::LOAN);

  const Tag phy_tag = att_.ToTag(phy_addr);
  if (!FullBlock(phy_addr, size)) {
    if (state == ATTEntry::DIRTY) {
      MoveToNVM(mach_base, phy_addr, att_.block_size());
    } else {
      MoveToDRAM(mach_base, phy_addr, att_.block_size());
    }
  }
  ATTSetup(att_, phy_tag, mach_base, state);
}

void AddrTransController::HideClean(int index, bool move_data) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::CLEAN);

  const Addr phy_addr = att_.ToAddr(entry.phy_tag);
  mem_store_->OnATTHideClean(phy_addr, move_data);

  if (move_data) {
    assert(!IsStatic(phy_addr));
    MoveToNVM(phy_addr, entry.mach_base, att_.block_size());
  }
  VBBackupBlock(nvm_buffer_, entry.mach_base, VersionBuffer::BACKUP0);
  ATTShiftState(att_, index, ATTEntry::HIDDEN);
}

Addr AddrTransController::ResetClean(int index, bool move_data) {
  assert(in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::CLEAN);

  mem_store_->OnATTResetClean(att_.ToAddr(entry.phy_tag), move_data);

  const Addr mach_base = dram_buffer_.NewBlock();
  if (move_data) {
    MoveToDRAM(mach_base, entry.mach_base, att_.block_size());
  }
  VBBackupBlock(nvm_buffer_, entry.mach_base, VersionBuffer::BACKUP1);
  ATTReset(att_, index, mach_base, ATTEntry::TEMP);
  return mach_base;
}

void AddrTransController::FreeClean(int index, bool move_data) {
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::CLEAN);

  Addr phy_addr = att_.ToAddr(entry.phy_tag);
  mem_store_->OnATTFreeClean(phy_addr, move_data);
 
  if (in_checkpointing()) {
    SwapNVM(phy_addr, entry.mach_base, att_.block_size());
  } else { // in running
    assert(!IsStatic(phy_addr));
    MoveToNVM(phy_addr, entry.mach_base, att_.block_size());
  }
  VBBackupBlock(nvm_buffer_, entry.mach_base,
      (VersionBuffer::State)in_checkpointing());
  ATTShiftState(att_, index, ATTEntry::FREE);
}

Addr AddrTransController::DirtyStained(int index, bool move_data) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::STAINED);

  const Addr mach_base = VBNewBlock(nvm_buffer_);
  if (move_data) {
    MoveToNVM(mach_base, entry.mach_base, att_.block_size());
  }
  VBFreeBlock(dram_buffer_, entry.mach_base, VersionBuffer::IN_USE);
  ATTReset(att_, index, mach_base, ATTEntry::DIRTY);
  return mach_base;
}

void AddrTransController::FreeLoan(int index, bool move_data) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::LOAN);

  const Addr phy_addr = att_.ToAddr(entry.phy_tag);
  mem_store_->OnATTFreeLoan(phy_addr, move_data);

  if (move_data) {
    assert(IsStatic(phy_addr));
    MoveToDRAM(phy_addr, entry.mach_base, att_.block_size());
  }
  VBFreeBlock(dram_buffer_, entry.mach_base, VersionBuffer::IN_USE);
  ATTShiftState(att_, index, ATTEntry::FREE);
}

void AddrTransController::HideTemp(int index, bool move_data) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::TEMP);

  const Addr phy_addr = att_.ToAddr(entry.phy_tag);
  if (move_data) {
    assert(!IsStatic(phy_addr));
    MoveToNVM(phy_addr, entry.mach_base, att_.block_size());
  }
  VBFreeBlock(dram_buffer_, entry.mach_base, VersionBuffer::IN_USE);
  ATTShiftState(att_, index, ATTEntry::HIDDEN);
}

