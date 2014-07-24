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

  PTTEntry* page = migrator_.LookupPage(phy_addr, Profiler::Null);
  if (!page) { // NVM
    int index = ATTLookup(att_, phy_tag).first;
    mem_store_->OnNVMRead(phy_addr, att_.block_size());
    if (index == -EINVAL) return phy_addr; // direct mapping

    const ATTEntry& entry = att_.At(index);
    Addr mach_addr = att_.Translate(phy_addr, entry.mach_base);
    att_.AddBlockRead(index);
    return mach_addr;
  } else { // DRAM
    Addr mach_addr = migrator_.Translate(phy_addr, page->mach_base);
    migrator_.AddDRAMPageRead(*page);
    mem_store_->OnDRAMRead(mach_addr, att_.block_size());
    return mach_addr;
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
        FreeClean(ATTFront(att_, ATTEntry::CLEAN));
      } else {
        mem_store_->OnATTFreeSetup(phy_addr, ATTEntry::LOAN);
      }
      const Addr mach_base = dram_buffer_.NewBlock();
      Setup(phy_addr, mach_base, size, ATTEntry::LOAN);
      return att_.Translate(phy_addr, mach_base);
    } else { // in running
      return phy_addr;
    }
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
      case ATTEntry::TEMP:
        HideTemp(index, !FullBlock(phy_addr, size));
      case ATTEntry::DIRTY:
      case ATTEntry::HIDDEN:
        mach_addr = att_.Translate(phy_addr, entry.mach_base);
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
        } else {
          assert(!att_.IsEmpty(ATTEntry::CLEAN));
          int ci = ATTFront(att_, ATTEntry::CLEAN);
          FreeClean(ci);
        }
      } else {
        mem_store_->OnATTFreeSetup(phy_addr, ATTEntry::DIRTY);
      }
      Addr mach_base = VBNewBlock(nvm_buffer_);
      index = Setup(phy_addr, mach_base, size, ATTEntry::DIRTY);
      mach_addr = att_.Translate(phy_addr, mach_base);
    }
    att_.AddBlockWrite(index);
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
        assert(!att_.IsEmpty(ATTEntry::CLEAN));
        FreeClean(ATTFront(att_, ATTEntry::CLEAN));
      } else {
        mem_store_->OnATTFreeSetup(phy_addr, ATTEntry::STAINED);
      }
      Addr mach_base = dram_buffer_.NewBlock();
      index = Setup(phy_addr, mach_base, size, ATTEntry::STAINED);
      mach_addr = att_.Translate(phy_addr, mach_base);
    }
    att_.AddBlockWrite(index);
    return mach_addr;
  }
}

Control AddrTransController::Probe(Addr phy_addr) {
  static int ptt_limit = migrator_.ptt_length() - (migrator_.ptt_length() >> 3);
  if (migrator_.Contains(phy_addr, Profiler::Null)) { // DRAM
    if (in_checkpointing()) {
      if (!att_.Contains(phy_addr) &&
          att_.IsEmpty(ATTEntry::FREE) && att_.IsEmpty(ATTEntry::CLEAN)) {
        return WAIT_CKPT;
      }
    }
  } else if (!att_.Contains(phy_addr)) { // NVM
    if (in_checkpointing()) {
      if (att_.IsEmpty(ATTEntry::FREE) && att_.IsEmpty(ATTEntry::CLEAN)) {
        return WAIT_CKPT;
      }
    } else if (att_.GetLength(ATTEntry::DIRTY) == att_.length() ||
        migrator_.num_dirty_entries() > ptt_limit) {
      return NEW_EPOCH;
    }
  }
  return REG_WRITE;
}

Addr AddrTransController::StoreAddr(Addr phy_addr, int size) {
  assert(CheckValid(phy_addr, size) && phy_addr < phy_range_);
  PTTEntry* page = migrator_.LookupPage(phy_addr, Profiler::Null); //TODO
  if (!page) {
    mem_store_->OnNVMStore(phy_addr, size);
    return NVMStore(phy_addr, size);
  } else {
    migrator_.AddDRAMPageWrite(*page);
    if (page->state == PTTEntry::CLEAN_STATIC) {
      migrator_.ShiftState(*page, PTTEntry::DIRTY_DIRECT, Profiler::Null); //TODO
    } else if (page->state == PTTEntry::CLEAN_DIRECT) {
      migrator_.ShiftState(*page, PTTEntry::DIRTY_STATIC, Profiler::Null); //TODO
    }
    mem_store_->OnDRAMStore(phy_addr, size);
    return DRAMStore(phy_addr, size);
  }
}

void AddrTransController::DirtyCleaner::Visit(int i) {
  const ATTEntry& entry = atc_->att_.At(i);
  if (entry.state == ATTEntry::STAINED) {
    atc_->DirtyStained(i);
  } else if (entry.state == ATTEntry::TEMP) {
    atc_->HideTemp(i);
  }

  if (entry.state == ATTEntry::DIRTY) {
    atc_->ATTShiftState(atc_->att_, i, ATTEntry::CLEAN);
    ++num_flushed_entries_;
  } else if (entry.state == ATTEntry::HIDDEN) {
    atc_->ATTShiftState(atc_->att_, i, ATTEntry::FREE);
    ++num_flushed_entries_;
  }
}

void AddrTransController::LoanRevoker::Visit(int i) {
  const ATTEntry& entry = atc_->att_.At(i);
  assert(entry.state == ATTEntry::LOAN);
  atc_->FreeLoan(i);
}

void AddrTransController::MigrateDRAM(const DRAMPageStats& stats,
    Profiler& profiler) {
  if (stats.state == PTTEntry::CLEAN_STATIC) {
    profiler.AddPageMoveIntra();
  } else if (stats.state == PTTEntry::DIRTY_DIRECT) {
    profiler.AddPageMoveInter(); // for write back
  } else if (stats.state == PTTEntry::DIRTY_DIRECT) {
    profiler.AddPageMoveIntra();
    profiler.AddPageMoveInter();
  }
#ifdef MEMCK
  Tag tag = att_.ToTag(stats.phy_addr);
  for (int i = 0; i < migrator_.page_blocks(); ++i) {
    assert(!att_.Contains(tag + i));
  }
#endif
  migrator_.Free(stats.phy_addr, profiler);
  profiler.AddTableOp();
}

void AddrTransController::MigrateNVM(const NVMPageStats& stats,
    Profiler& profiler) {
  Tag phy_tag = att_.ToTag(stats.phy_addr);
  Tag limit = att_.ToTag(stats.phy_addr + migrator_.page_size());
  assert(limit - phy_tag == 1 << (migrator_.page_bits() - att_.block_bits()));

  for (Tag tag = phy_tag; tag < limit; ++tag) {
    int index = ATTLookup(att_, tag).first;
    if (index == -EINVAL) continue;
    const ATTEntry& entry = att_.At(index);
    // copy data to DRAM
    mem_store_->DoMove(stats.phy_addr, entry.mach_base, att_.block_size());
    switch (entry.state) {
      case ATTEntry::CLEAN:
      case ATTEntry::DIRTY:
        Discard(index, nvm_buffer_, profiler);
        break;
      case ATTEntry::STAINED:
      case ATTEntry::TEMP:
        Discard(index, dram_buffer_, profiler);
        break;
      case ATTEntry::LOAN:
        assert(entry.state != ATTEntry::LOAN);
      case ATTEntry::FREE:
        assert(entry.state != ATTEntry::FREE);
      case ATTEntry::HIDDEN:
        break;
    }
  }
  if (stats.dirty_ratio > 0.5) {
    migrator_.Setup(stats.phy_addr, PTTEntry::DIRTY_STATIC, profiler);
  } else {
    migrator_.Setup(stats.phy_addr, PTTEntry::DIRTY_DIRECT, profiler);
  }
}

void AddrTransController::MigratePages(Profiler& profiler,
    double dr, double wr) {
  assert(!in_checkpointing());

  LoanRevoker loan_revoker(this);
  ATTVisit(att_, ATTEntry::LOAN, &loan_revoker);
  assert(att_.IsEmpty(ATTEntry::LOAN));

  migrator_.InputBlocks(att_.entries());

  NVMPageStats n;
  DRAMPageStats d;
  bool d_ready = false;
  while (migrator_.ExtractNVMPage(n, profiler)) {
    if (n.dirty_ratio < dr) break;
    // Find a DRAM page for exchange
    if (migrator_.num_entries() == migrator_.ptt_length()) {
      if (!migrator_.ExtractDRAMPage(d, Profiler::Overlap)) return;
      if (d.write_ratio > 0) {
        d_ready = true;
        break;
      }
      MigrateDRAM(d, profiler);
    }
    MigrateNVM(n, profiler);
  }
  if (!d_ready && !migrator_.ExtractDRAMPage(d, profiler)) return;
  do {
    if (d.write_ratio > wr) break;
    MigrateDRAM(d, profiler);
  } while (migrator_.ExtractDRAMPage(d, profiler));
}

void AddrTransController::BeginCheckpointing(Profiler& profiler) {
  assert(!in_checkpointing());

  // ATT flush
  DirtyCleaner att_cleaner(this);
  ATTVisit(att_, ATTEntry::DIRTY, &att_cleaner);
  assert(att_.GetLength(ATTEntry::CLEAN) +
      att_.GetLength(ATTEntry::FREE) == att_.length());
  profiler.AddTableOp(); // suppose in parallel

  in_checkpointing_ = true;

  att_.ClearStats();
  migrator_.Clear(profiler);
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
  ATTReset(att_, index, phy_addr, ATTEntry::HIDDEN);
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
  ATTReset(att_, index, phy_addr, ATTEntry::HIDDEN);
}

void AddrTransController::Discard(int index, VersionBuffer& vb,
    Profiler& profiler) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  FreeBlock(vb, entry.mach_base, VersionBuffer::IN_USE, profiler);
  ShiftState(index, ATTEntry::FREE, profiler);
}

