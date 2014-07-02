// addr_trans_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "addr_trans_controller.h"

using namespace std;

Addr AddrTransController::LoadAddr(Addr phy_addr) {
  assert(phy_addr < PhyLimit());
  Tag phy_tag = att_.ToTag(phy_addr);
  Addr mach_addr = att_.Translate(phy_addr, ATTLookup(att_, phy_tag).second);
  if (IsVolatile(phy_addr)) {
    mem_store_->OnDRAMRead(mach_addr, att_.block_size());
  } else {
    mem_store_->OnNVMRead(mach_addr, att_.block_size());
  }
  return mach_addr;
}

Addr AddrTransController::NVMStore(Addr phy_addr, int size) {
  const Tag phy_tag = att_.ToTag(phy_addr);
  int index = ATTLookup(att_, phy_tag).first;
  if (!in_checkpointing()) {
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.At(index);
      switch(entry.state) {
      case ATTEntry::DIRTY:
        return att_.Translate(phy_addr, entry.mach_base);
      case ATTEntry::TEMP:
        HideTemp(index, !FullBlock(phy_addr, size));
        return phy_addr;
      case ATTEntry::HIDDEN:
        return phy_addr;
      case ATTEntry::STAINED:
        return att_.Translate(phy_addr,
            DirtyStained(index, !FullBlock(phy_addr, size)));
      default:
        HideClean(index, !FullBlock(phy_addr, size));
        return phy_addr;
      }
    } else { // not found
      if (att_.IsEmpty(ATTEntry::FREE)) {
        if (!att_.IsEmpty(ATTEntry::LOAN)) {
          int li = ATTFront(att_, ATTEntry::LOAN);
          FreeLoan(li);
        } else {
          assert(!att_.IsEmpty(ATTEntry::CLEAN));
          int ci = ATTFront(att_, ATTEntry::CLEAN);
          FreeClean(ci);
        }
      }
      Addr mach_base = VBNewBlock(nvm_buffer_);
      Setup(phy_addr, mach_base, size, ATTEntry::DIRTY);
      return att_.Translate(phy_addr, mach_base);
    }
  } else { // in checkpointing
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.At(index);
      if (entry.state == ATTEntry::TEMP || entry.state == ATTEntry::DIRTY) {
        return att_.Translate(phy_addr, entry.mach_base);
      } else {
        Addr mach_base = ResetClean(index, !FullBlock(phy_addr, size));
        return att_.Translate(phy_addr, mach_base);
      }
    } else { // not found
      if (att_.IsEmpty(ATTEntry::FREE)) {
        assert(!att_.IsEmpty(ATTEntry::CLEAN));
        int ci = ATTFront(att_, ATTEntry::CLEAN);
        FreeClean(ci);
      }
      Addr mach_base = dram_buffer_.NewBlock();
      Setup(phy_addr, mach_base, size, ATTEntry::STAINED);
      return att_.Translate(phy_addr, mach_base);
    }
  }
}

Addr AddrTransController::DRAMStore(Addr phy_addr, int size) {
  int index = ATTLookup(att_, att_.ToTag(phy_addr)).first;
  if (index != -EINVAL) { // found
    const ATTEntry& entry = att_.At(index);
    if (in_checkpointing()) {
      return att_.Translate(phy_addr, entry.mach_base);
    } else {
      FreeLoan(index, !FullBlock(phy_addr, size));
      return phy_addr;
    }
  } else { // not found
    if (in_checkpointing()) {
      if (att_.IsEmpty(ATTEntry::FREE)) {
        assert(!att_.IsEmpty(ATTEntry::CLEAN));
        int ci = ATTFront(att_, ATTEntry::CLEAN);
        FreeClean(ci);
        // suppose the mapping is marked in NV-ATT
      }
      const Addr mach_base = dram_buffer_.NewBlock();
      Setup(phy_addr, mach_base, size, ATTEntry::LOAN);
      return att_.Translate(phy_addr, mach_base);
    } else { // in running
      return phy_addr;
    }
  }
}

void AddrTransController::PseudoPageStore(Tag phy_tag) {
  const pair<int, Addr> target = ATTLookup(ptt_, phy_tag);

  if (target.first != -EINVAL) { // found
    const ATTEntry &entry = ptt_.At(target.first);
    if (entry.state == ATTEntry::DIRTY ||
        entry.state == ATTEntry::HIDDEN) {
      return;
    } else {
      assert(entry.state == ATTEntry::CLEAN);
      ATTShiftState(ptt_, target.first, ATTEntry::HIDDEN);
    }
  } else {
    if (ptt_.IsEmpty(ATTEntry::FREE)) {
      assert(!ptt_.IsEmpty(ATTEntry::CLEAN));
      int ci = ATTFront(ptt_, ATTEntry::CLEAN);

      ++pages_twice_written_;
      ATTShiftState(ptt_, ci, ATTEntry::FREE);
    }
    ATTSetup(ptt_, phy_tag, INVAL_ADDR, ATTEntry::DIRTY);
  }
}

Addr AddrTransController::StoreAddr(Addr phy_addr, int size) {
  assert(CheckValid(phy_addr, size) && phy_addr < PhyLimit());
  if (IsVolatile(phy_addr)) {
    PseudoPageStore(ptt_.ToTag(phy_addr));
    return DRAMStore(phy_addr, size);
  } else {
    return NVMStore(phy_addr, size);
  }
}

AddrTransController::Control AddrTransController::Probe(Addr phy_addr) {
  if (IsVolatile(phy_addr)) {
    bool ptt_avail = ptt_.Contains(phy_addr) ||
        ptt_.GetLength(ATTEntry::DIRTY) < ptt_.length();  // inc. HIDDEN
    if (!in_checkpointing() && !ptt_avail) {
      return EPOCH;
    }
    bool att_avail = att_.Contains(phy_addr) ||
        !att_.IsEmpty(ATTEntry::FREE) || !att_.IsEmpty(ATTEntry::CLEAN);
    if (in_checkpointing() && (!ptt_avail || !att_avail)) {
      return RETRY;
    }
  } else { // NVM
    if (!in_checkpointing() && !att_.Contains(phy_addr) &&
        att_.GetLength(ATTEntry::DIRTY) == att_.length()) { // inc. HIDDEN, TEMP
      return EPOCH;
    }
    if (in_checkpointing() && !att_.Contains(phy_addr) &&
        att_.IsEmpty(ATTEntry::FREE) && att_.IsEmpty(ATTEntry::CLEAN)) {
      return RETRY;
    }
  }
  return ACCEPT;
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
}

void AddrTransController::FinishCheckpointing() {
  assert(in_checkpointing());
  nvm_buffer_.ClearBackup();
  in_checkpointing_ = false;
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
  if (move_data) {
    assert(!IsVolatile(phy_addr));
    MoveToNVM(phy_addr, entry.mach_base, att_.block_size());
  }
  VBBackupBlock(nvm_buffer_, entry.mach_base, VersionBuffer::BACKUP0);
  ATTShiftState(att_, index, ATTEntry::HIDDEN);
}

Addr AddrTransController::ResetClean(int index, bool move_data) {
  assert(in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::CLEAN);

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
  if (in_checkpointing()) {
    SwapNVM(phy_addr, entry.mach_base, att_.block_size());
  } else { // in running
    assert(!IsVolatile(phy_addr));
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
  if (move_data) {
    assert(IsVolatile(phy_addr));
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
    assert(!IsVolatile(phy_addr));
    MoveToNVM(phy_addr, entry.mach_base, att_.block_size());
  }
  VBFreeBlock(dram_buffer_, entry.mach_base, VersionBuffer::IN_USE);
  ATTShiftState(att_, index, ATTEntry::HIDDEN);
}
