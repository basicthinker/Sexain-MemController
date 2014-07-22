// addr_trans_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "addr_trans_controller.h"
#include <vector>

using namespace std;

// Space partition (low -> high):
// phy_limit || NVM buffer || DRAM buffer
AddrTransController::AddrTransController(
    uint64_t phy_range, uint64_t dram_size,
    int att_len, int block_bits, int page_bits, MemStore* ms):

    att_(att_len, block_bits), nvm_buffer_(2 * att_len, block_bits),
    dram_buffer_(att_len, block_bits),
    migrator_(block_bits, page_bits, dram_size >> page_bits),
    phy_range_(phy_range), dram_size_(dram_size) {

  assert(phy_range >= dram_size);
  mem_store_ = ms;
  in_checkpointing_ = false;

  nvm_buffer_.set_addr_base(phy_range_);
  dram_buffer_.set_addr_base(nvm_buffer_.addr_base() + nvm_buffer_.Size());
}

Addr AddrTransController::LoadAddr(Addr phy_addr) {
  assert(phy_addr < phy_range_);
  Tag phy_tag = att_.ToTag(phy_addr);

  PTTEntryPtr p = migrator_.LookupPage(phy_addr, Profiler::Null);
  if (migrator_.IsValid(p)) {
    Addr mach_addr = migrator_.Translate(phy_addr, p->first); // static mapping
    migrator_.AddDRAMPageRead(p);
    mem_store_->OnDRAMRead(mach_addr, att_.block_size());
    return mach_addr;
  } else {
    int index = ATTLookup(att_, phy_tag).first;
    mem_store_->OnNVMRead(phy_addr, att_.block_size());
    if (index == -EINVAL) return phy_addr; // direct mapping

    const ATTEntry& entry = att_.At(index);
    Addr mach_addr = att_.Translate(phy_addr, entry.mach_base);
    att_.AddBlockRead(index);
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
    mem_store_->OnDRAMWrite(mach_addr, size);
    return mach_addr;
  } else { // not found
    if (in_checkpointing()) {
      if (att_.IsEmpty(ATTEntry::FREE)) {
        if (att_.IsEmpty(ATTEntry::CLEAN)) return INVAL_ADDR;
        FreeClean(ATTFront(att_, ATTEntry::CLEAN));
      } else {
        mem_store_->OnATTFreeSetup(phy_addr, ATTEntry::LOAN);
      }
      const Addr mach_base = dram_buffer_.NewBlock();
      Setup(phy_addr, mach_base, size, ATTEntry::LOAN);
      mach_addr = att_.Translate(phy_addr, mach_base);
    } else { // in running
      mach_addr = phy_addr;
    }
    mem_store_->OnDRAMWrite(mach_addr, size);
    return mach_addr;
  }
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
      } else {
        mem_store_->OnATTFreeSetup(phy_addr, ATTEntry::DIRTY);
      }
      Addr mach_base = VBNewBlock(nvm_buffer_);
      index = Setup(phy_addr, mach_base, size, ATTEntry::DIRTY);
      mach_addr = att_.Translate(phy_addr, mach_base);
    }
    att_.AddBlockWrite(index);
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
        if (att_.IsEmpty(ATTEntry::CLEAN)) return INVAL_ADDR;
        FreeClean(ATTFront(att_, ATTEntry::CLEAN));
      } else {
        mem_store_->OnATTFreeSetup(phy_addr, ATTEntry::STAINED);
      }
      Addr mach_base = dram_buffer_.NewBlock();
      index = Setup(phy_addr, mach_base, size, ATTEntry::STAINED);
      mach_addr = att_.Translate(phy_addr, mach_base);
    }
    att_.AddBlockWrite(index);
    mem_store_->OnDRAMWrite(mach_addr, size);
    return mach_addr;
  }
}

Addr AddrTransController::StoreAddr(Addr phy_addr, int size) {
  assert(CheckValid(phy_addr, size) && phy_addr < phy_range_);
  PTTEntryPtr p = migrator_.LookupPage(phy_addr, Profiler::Null); //TODO
  if (migrator_.IsValid(p)) { // found
    Addr mach_addr = DRAMStore(phy_addr, size);
    if (mach_addr != INVAL_ADDR) {
      migrator_.AddDRAMPageWrite(p);
      if (p->second.state == PTTEntry::CLEAN_STATIC) {
        migrator_.ShiftState(p, PTTEntry::DIRTY_DIRECT, Profiler::Null); //TODO
      } else if (p->second.state == PTTEntry::CLEAN_DIRECT) {
        migrator_.ShiftState(p, PTTEntry::DIRTY_STATIC, Profiler::Null); //TODO
      }
    }
    return mach_addr;
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

void AddrTransController::MigrateDRAM(const PageStats& stats,
    Profiler& profiler) {
  if (stats.state == PTTEntry::CLEAN_STATIC) {
    profiler.AddPageMoveIntra();
  } else if (stats.state == PTTEntry::DIRTY_DIRECT) {
    profiler.AddPageMoveInter(); // for write back
  } else if (stats.state == PTTEntry::DIRTY_DIRECT) {
    profiler.AddPageMoveIntra();
    profiler.AddPageMoveInter();
  }
  migrator_.Free(stats.phy_addr, profiler);
  profiler.AddTableOp();
}

void AddrTransController::MigrateNVM(const PageStats& stats,
    Profiler& profiler) {
  Tag phy_tag = att_.ToTag(stats.phy_addr);
  Tag limit = att_.ToTag(stats.phy_addr + migrator_.page_size());
  assert(limit - phy_tag == 1 << (migrator_.page_bits() - att_.block_bits()));

  for (Tag tag = phy_tag; tag < limit; ++tag) {
    int index = ATTLookup(att_, tag).first;
    if (index == -EINVAL) continue;
    const ATTEntry& entry = att_.At(index);
    if (entry.state != ATTEntry::HIDDEN) {
      assert(entry.state != ATTEntry::LOAN);
      mem_store_->DoMove(stats.phy_addr, entry.mach_base, att_.block_size());
    }
    att_.ShiftState(index, ATTEntry::FREE);
  }
  if (stats.dirty_ratio > 0.5) {
    migrator_.Setup(stats.phy_addr, PTTEntry::DIRTY_STATIC, profiler);
  } else {
    migrator_.Setup(stats.phy_addr, PTTEntry::DIRTY_DIRECT, profiler);
  }
}

void AddrTransController::MigratePages(double threshold, Profiler& profiler) {
  assert(!in_checkpointing());

  LoanRevoker loan_revoker(this);
  ATTVisit(att_, ATTEntry::LOAN, &loan_revoker);
  assert(att_.IsEmpty(ATTEntry::LOAN));

  PageStats n, d;
  while (migrator_.ExtractNVMPage(n, profiler) && n.dirty_ratio > threshold) {
    if (!migrator_.ExtractDRAMPage(d, Profiler::Overlap)) return;
    if (d.dirty_ratio > 0) break;
    MigrateDRAM(d, profiler);
    MigrateNVM(n, profiler);
  }
  do {
    if (d.write_ratio < 1) {
      MigrateDRAM(d, profiler);
    } else break;
  } while (migrator_.ExtractDRAMPage(d, profiler));

  att_.ClearStats();
  migrator_.Clear(profiler);
}

void AddrTransController::BeginCheckpointing() {
  assert(!in_checkpointing());

  // ATT flush
  DirtyCleaner att_cleaner(this);
  ATTVisit(att_, ATTEntry::DIRTY, &att_cleaner);
  assert(att_.GetLength(ATTEntry::CLEAN) +
      att_.GetLength(ATTEntry::FREE) == att_.length());

  in_checkpointing_ = true;
  mem_store_->OnCheckpointing(
      att_cleaner.num_new_entries(), 0);
}

void AddrTransController::FinishCheckpointing() {
  assert(in_checkpointing());
  nvm_buffer_.ClearBackup();
  in_checkpointing_ = false;
  mem_store_->OnEpochEnd();
}

int AddrTransController::Setup(Addr phy_addr, Addr mach_base, int size,
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
  return ATTSetup(att_, phy_tag, mach_base, state);
}

void AddrTransController::HideClean(int index, bool move_data) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::CLEAN);

  const Addr phy_addr = att_.ToAddr(entry.phy_tag);
  mem_store_->OnATTHideClean(phy_addr, move_data);

  if (move_data) {
    assert(!IsDRAM(phy_addr));
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
    assert(!IsDRAM(phy_addr));
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
    assert(IsDRAM(phy_addr));
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
    assert(!IsDRAM(phy_addr));
    MoveToNVM(phy_addr, entry.mach_base, att_.block_size());
  }
  VBFreeBlock(dram_buffer_, entry.mach_base, VersionBuffer::IN_USE);
  ATTShiftState(att_, index, ATTEntry::HIDDEN);
}

