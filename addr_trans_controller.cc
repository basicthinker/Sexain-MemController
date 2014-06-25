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
  const Tag phy_tag = att_.ToTag(phy_addr);
  int index = att_.Lookup(phy_tag).first;
  if (!in_checkpointing()) {
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.At(index);
      if (entry.state == ATTEntry::REG_DIRTY) {
        return att_.Translate(phy_addr, entry.mach_base);
      } else if (entry.state == ATTEntry::CROSS_DIRTY) {
        Addr mach_base = RegularizeDirty(index, !FullBlock(phy_addr, size));
        return att_.Translate(phy_addr, mach_base);;
      } else if (entry.state == ATTEntry::CROSS_TEMP) {
        CleanCrossTemp(index, !FullBlock(phy_addr, size));
        return phy_addr;
      } else if (entry.state == ATTEntry::HIDDEN_CLEAN) {
        return phy_addr;
      } else {
        HideClean(index, !FullBlock(phy_addr, size));
        return phy_addr;
      }
    } else { // not found
      if (att_.IsEmpty(ATTEntry::FREE)) {
        if (!att_.IsEmpty(ATTEntry::REG_TEMP)) {
          int ci = att_.GetFront(ATTEntry::REG_TEMP);
          FreeRegTemp(ci);
        } else {
          assert(!att_.IsEmpty(ATTEntry::VISIBLE_CLEAN));
          int ti = att_.GetFront(ATTEntry::VISIBLE_CLEAN);
          FreeVisibleClean(ti);
        }
      }
      Addr mach_base = nvm_buffer_.NewBlock();
      Setup(phy_addr, mach_base, size, ATTEntry::REG_DIRTY);
      return att_.Translate(phy_addr, mach_base);
    }
  } else { // in checkpointing
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.At(index);
      if (entry.state == ATTEntry::CROSS_TEMP ||
          entry.state == ATTEntry::CROSS_DIRTY) {
        return att_.Translate(phy_addr, entry.mach_base);
      } else {
        Addr mach_base = ResetVisibleClean(index, !FullBlock(phy_addr, size));
        return att_.Translate(phy_addr, mach_base);
      }
    } else { // not found
      if (att_.IsEmpty(ATTEntry::FREE)) {
        assert(!att_.IsEmpty(ATTEntry::VISIBLE_CLEAN));
        int ci = att_.GetFront(ATTEntry::VISIBLE_CLEAN);
        FreeVisibleClean(ci);
      }
      Addr mach_base = dram_buffer_.NewBlock();
      Setup(phy_addr, mach_base, size, ATTEntry::CROSS_DIRTY);
      return att_.Translate(phy_addr, mach_base);
    }
  }
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
        FreeVisibleClean(ci);
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
      ptt_.ShiftState(ci, ATTEntry::FREE);
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

AddrTransController::Control AddrTransController::Probe(Addr phy_addr) {
  if (IsDRAM(phy_addr)) {
    bool ptt_avail = ptt_.Contains(phy_addr) ||
        ptt_.GetLength(ATTEntry::REG_DIRTY) < ptt_.length();
    if (in_checkpointing()) {
      bool att_avail = att_.Contains(phy_addr) ||
          att_.GetLength(ATTEntry::REG_DIRTY) < att_.length();
      if (!att_avail || !ptt_avail) {
        return RETRY;
      }
    } else if (!ptt_avail) {
      return EPOCH;
    }
  } else { // NVM
    if (in_checkpointing() && !att_.Contains(phy_addr) &&
        att_.IsEmpty(ATTEntry::VISIBLE_CLEAN)) {
      return RETRY;
    } else if (!in_checkpointing() && !att_.Contains(phy_addr) &&
        att_.GetLength(ATTEntry::REG_DIRTY) == att_.length()) {
      return EPOCH;
    }
  }
  return ACCEPT;
}

void AddrTransController::BeginCheckpointing() {
  assert(!in_checkpointing());

  DirtyCleaner att_cleaner(*this);
  att_.VisitQueue(ATTEntry::REG_DIRTY, &att_cleaner);
  assert(att_.IsEmpty(ATTEntry::REG_DIRTY));

  TempRevoker temp_revoker(*this);
  att_.VisitQueue(ATTEntry::REG_TEMP, &temp_revoker);
  assert(att_.IsEmpty(ATTEntry::REG_TEMP));
  assert(att_.GetLength(ATTEntry::VISIBLE_CLEAN) +
      att_.GetLength(ATTEntry::FREE) == att_.length());

  PTTCleaner ptt_cleaner(ptt_);
  ptt_.VisitQueue(ATTEntry::REG_DIRTY, &ptt_cleaner);
  assert(ptt_.IsEmpty(ATTEntry::REG_DIRTY));
  ptt_buffer_.FreeBackup(); // PTT supposes the next epoch immediately begins

  in_checkpointing_ = true;
}

void AddrTransController::FinishCheckpointing() {
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
  att_.ShiftState(index, ATTEntry::FREE);
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
  att_.ShiftState(index, ATTEntry::FREE);
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
