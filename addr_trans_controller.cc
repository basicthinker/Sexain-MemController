// addr_trans_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include <vector>
#include "addr_trans_controller.h"
#include "base/trace.hh"
#include "debug/Migration.hh"

using namespace std;
using namespace thynvm;

// Space partition (low -> high):
// phy_limit || NVM buffer || DRAM buffer
AddrTransController::AddrTransController(
    uint64_t phy_range, uint64_t dram_size,
    int att_len, int block_bits, int page_bits, MemStore* ms):

    att_(att_len, block_bits), nvm_buffer_(2 * att_len, block_bits),
    dram_buffer_(att_len, block_bits),
    migrator_(block_bits, page_bits, dram_size >> page_bits),
    phy_range_(phy_range), dram_size_(dram_size),
    pages_to_dram_(0), pages_to_nvm_(0) {

  assert(phy_range >= dram_size);
  mem_store_ = ms;
  in_checkpointing_ = false;

  nvm_buffer_.setAddrBase(phy_range_);
  dram_buffer_.setAddrBase(nvm_buffer_.addrBase() + nvm_buffer_.size());
}

Addr AddrTransController::LoadAddr(Addr phy_addr, Profiler& pf) {
  int index = att_.lookup(att_.toTag(phy_addr), pf);
  if (index != -EINVAL) {
    const ATTEntry& entry = att_.at(index);
    att_.addReadCount(index);
    Addr mach_addr = att_.toHardwareAddr(phy_addr, entry.hw_addr);
    // bool is_dram = (entry.state == ATTEntry::PRE_HIDDEN ||
    //     entry.state == ATTEntry::LOAN ||
    //     entry.state == ATTEntry::PRE_DIRTY);
    // pf.addLatency(mem_store_->GetReadLatency(mach_addr, is_dram));
    return mach_addr;
  } else {
    PTTEntry* page = migrator_.lookupPage(phy_addr, pf);
    Addr mach_addr;
    if (page) {
      migrator_.AddDRAMPageRead(*page);
      mach_addr = migrator_.toHardwareAddr(phy_addr, page->hw_addr);
      // pf.addLatency(mem_store_->GetReadLatency(mach_addr, true));
    } else {
      mach_addr = phy_addr;
      // pf.addLatency(mem_store_->GetReadLatency(mach_addr, false));
    }
    return mach_addr;
  }
}

Addr AddrTransController::DRAMStore(Addr phy_addr, int size, Profiler& pf) {
  int index = att_.lookup(att_.toTag(phy_addr), pf);
  if (index != -EINVAL) { // found
    const ATTEntry& entry = att_.at(index);
    // mem_store_->OnATTWriteHit(entry.state);
    if (in_checkpointing()) {
      return att_.toHardwareAddr(phy_addr, entry.hw_addr);
    } else {
      FreeLoan(index, !FullBlock(phy_addr, size), pf);
      return phy_addr;
    }
  } else { // not found
    if (in_checkpointing()) {
      if (att_.isEmpty(ATTEntry::FREE)) {
        assert(!att_.isEmpty(ATTEntry::CLEAN));
        FreeClean(att_.getFront(ATTEntry::CLEAN), pf);
        // mem_store_->OnATTWriteMiss(ATTEntry::LOAN);
      } else {
        // mem_store_->OnATTWriteHit(ATTEntry::LOAN);
      }
      const Addr hw_addr = dram_buffer_.allocSlot(Profiler::Overlap);
      insert(phy_addr, hw_addr, ATTEntry::LOAN, !FullBlock(phy_addr, size), pf);
      return att_.toHardwareAddr(phy_addr, hw_addr);
    } else { // in running
      return phy_addr;
    }
  }
}

Addr AddrTransController::NVMStore(Addr phy_addr, int size, Profiler& pf) {
  const Tag phy_tag = att_.toTag(phy_addr);
  int index = att_.lookup(phy_tag, pf);
  Addr mach_addr;
  if (!in_checkpointing()) {
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.at(index);
      // mem_store_->OnATTWriteHit(entry.state);
      switch(entry.state) {
      case ATTEntry::PRE_HIDDEN:
        HideTemp(index, !FullBlock(phy_addr, size), pf);
      case ATTEntry::DIRTY:
      case ATTEntry::HIDDEN:
        mach_addr = att_.toHardwareAddr(phy_addr, entry.hw_addr);
        break;
      case ATTEntry::PRE_DIRTY:
        mach_addr = att_.toHardwareAddr(phy_addr,
            DirtyStained(index, !FullBlock(phy_addr, size), pf));
        break;
      default:
        HideClean(index, !FullBlock(phy_addr, size), pf);
        mach_addr = phy_addr;
        break;
      }
    } else { // not found
      if (att_.isEmpty(ATTEntry::FREE)) {
        if (!att_.isEmpty(ATTEntry::LOAN)) {
          int li = att_.getFront(ATTEntry::LOAN);
          FreeLoan(li, true, pf);
        } else {
          assert(!att_.isEmpty(ATTEntry::CLEAN));
          int ci = att_.getFront(ATTEntry::CLEAN);
          FreeClean(ci, pf);
        }
        // mem_store_->OnATTWriteMiss(ATTEntry::DIRTY);
      } else {
        // mem_store_->OnATTWriteHit(ATTEntry::DIRTY);
      }
      Addr hw_addr = nvm_buffer_.allocSlot(Profiler::Overlap);
      index = insert(phy_addr, hw_addr, ATTEntry::DIRTY,
          !FullBlock(phy_addr, size), pf);
      mach_addr = att_.toHardwareAddr(phy_addr, hw_addr);
    }
    att_.addWriteCount(index);
    return mach_addr;
  } else { // in checkpointing
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.at(index);
      // mem_store_->OnATTWriteHit(entry.state);
      if (entry.state == ATTEntry::PRE_HIDDEN || entry.state == ATTEntry::PRE_DIRTY) {
        mach_addr = att_.toHardwareAddr(phy_addr, entry.hw_addr);
      } else {
        Addr hw_addr = ResetClean(index, !FullBlock(phy_addr, size), pf);
        mach_addr = att_.toHardwareAddr(phy_addr, hw_addr);
      }
    } else { // not found
      if (att_.isEmpty(ATTEntry::FREE)) {
        assert(!att_.isEmpty(ATTEntry::CLEAN));
        FreeClean(att_.getFront(ATTEntry::CLEAN), pf);
        // mem_store_->OnATTWriteMiss(ATTEntry::PRE_DIRTY);
      } else {
        // mem_store_->OnATTWriteHit(ATTEntry::PRE_DIRTY);
      }
      Addr hw_addr = dram_buffer_.allocSlot(Profiler::Overlap);
      index = insert(phy_addr, hw_addr, ATTEntry::PRE_DIRTY,
          !FullBlock(phy_addr, size), pf);
      mach_addr = att_.toHardwareAddr(phy_addr, hw_addr);
    }
    att_.addWriteCount(index);
    return mach_addr;
  }
}

Control AddrTransController::Probe(Addr phy_addr) {
  if (migrator_.contains(phy_addr, Profiler::Null)) { // DRAM
    if (in_checkpointing()) {
      if (!att_.contains(phy_addr, Profiler::Null) &&
          att_.isEmpty(ATTEntry::FREE) && att_.isEmpty(ATTEntry::CLEAN)) {
        return WAIT_CKPT;
      }
    }
  } else if (!att_.contains(phy_addr, Profiler::Null)) { // NVM
    if (in_checkpointing()) {
      if (att_.isEmpty(ATTEntry::FREE) && att_.isEmpty(ATTEntry::CLEAN)) {
        return WAIT_CKPT;
      }
    } else if (att_.getLength(ATTEntry::DIRTY) == att_.length() ||
         migrator_.num_dirty_entries() >= migrator_.ptt_length() ) {
      return NEW_EPOCH;
    }
  }
  return REG_WRITE;
}

Addr AddrTransController::StoreAddr(Addr phy_addr, int size, Profiler& pf) {
  assert(CheckValid(phy_addr, size) && phy_addr < phy_range_);
  PTTEntry* page = migrator_.lookupPage(phy_addr, pf);
  if (!page) {
    Addr mach_addr = NVMStore(phy_addr, size, pf);
    // pf.addLatency(mem_store_->GetWriteLatency(mach_addr, false));
    return mach_addr;
  } else {
    migrator_.AddDRAMPageWrite(*page);
    if (page->state == PTTEntry::CLEAN_STATIC) {
      migrator_.shiftState(*page, PTTEntry::DIRTY_DIRECT, pf);
    } else if (page->state == PTTEntry::CLEAN_DIRECT) {
      migrator_.shiftState(*page, PTTEntry::DIRTY_STATIC, pf);
    }
    Addr mach_addr = DRAMStore(phy_addr, size, pf);
    // pf.addLatency(mem_store_->GetWriteLatency(mach_addr, true));
    return mach_addr;
  }
}

void AddrTransController::DirtyCleaner::Visit(int i) {
  const ATTEntry& entry = atc_->att_.at(i);
  if (entry.state == ATTEntry::PRE_DIRTY) {
    atc_->DirtyStained(i, true, pf_);
  } else if (entry.state == ATTEntry::PRE_HIDDEN) {
    atc_->HideTemp(i, true, pf_);
  }

  if (entry.state == ATTEntry::DIRTY) {
    atc_->att_.shiftState(i, ATTEntry::CLEAN, Profiler::Overlap);
    ++num_flushed_entries_;
  } else if (entry.state == ATTEntry::HIDDEN) {
    atc_->att_.shiftState(i, ATTEntry::FREE, Profiler::Overlap);
    ++num_flushed_entries_;
  }
}

void AddrTransController::LoanRevoker::Visit(int i) {
  const ATTEntry& entry = atc_->att_.at(i);
  assert(entry.state == ATTEntry::LOAN);
  atc_->FreeLoan(i, true, pf_);
}

void AddrTransController::MigrateDRAM(const DRAMPageStats& stats, Profiler& pf) {
  DPRINTF(Migration, "Migrate DRAM page WR=%f, from %s.\n",
      stats.write_ratio, PTTEntry::state_strings[stats.state]);
  if (stats.state == PTTEntry::CLEAN_STATIC) {
    pf.addPageIntraChannel();
  } else if (stats.state == PTTEntry::DIRTY_DIRECT) {
    pf.addPageInterChannel(); // for write back
  } else if (stats.state == PTTEntry::DIRTY_STATIC) {
    pf.addPageIntraChannel();
    pf.addPageInterChannel();
  }
#ifdef MEMCK
  Tag tag = att_.toTag(stats.phy_addr);
  for (int i = 0; i < migrator_.page_blocks(); ++i) {
    assert(!att_.contains(tag + i, Profiler::Null));
  }
#endif
  migrator_.Free(stats.phy_addr, pf);
}

void AddrTransController::MigrateNVM(const NVMPageStats& stats, Profiler& pf) {
  Tag phy_tag = att_.toTag(stats.phy_addr);
  Tag limit = att_.toTag(stats.phy_addr + migrator_.page_size());
  assert(limit - phy_tag == migrator_.page_blocks());

  for (Tag tag = phy_tag; tag < limit; ++tag) {
    int index = att_.lookup(tag, pf);
    if (index == -EINVAL) {
      pf.addBlockInterChannel(); // for copying data to DRAM
      continue;
    }
    const ATTEntry& entry = att_.at(index);

    pf.setIgnoreLatency();
    switch (entry.state) {
      case ATTEntry::CLEAN:
      case ATTEntry::DIRTY:
        pf.addBlockInterChannel(); // for copying data to DRAM
        CopyBlockIntra(att_.toAddr(entry.phy_tag), entry.hw_addr, pf);
        Discard(index, nvm_buffer_, pf);
        break;
      case ATTEntry::PRE_DIRTY:
      case ATTEntry::PRE_HIDDEN:
        pf.addBlockIntraChannel(); // for copying data to DRAM
        CopyBlockInter(att_.toAddr(entry.phy_tag), entry.hw_addr, pf);
        Discard(index, dram_buffer_, pf);
        break;
      case ATTEntry::LOAN:
        assert(entry.state != ATTEntry::LOAN);
      case ATTEntry::FREE:
        assert(entry.state != ATTEntry::FREE);
      case ATTEntry::HIDDEN:
        pf.addBlockInterChannel(); // for copying data to DRAM
        att_.shiftState(index, ATTEntry::FREE, pf);
        break;
    }
    pf.clearIgnoreLatency();
  }
  if (stats.dirty_ratio > 0.5) {
    migrator_.insert(stats.phy_addr, PTTEntry::CLEAN_STATIC, pf);
    DPRINTF(Migration, "Migrate NVM page to CLEAN_STATIC.\n");
  } else {
    migrator_.insert(stats.phy_addr, PTTEntry::CLEAN_DIRECT, pf);
    DPRINTF(Migration, "Migrate NVM page to CLEAN_DIRECT.\n");
  }
  ++pages_to_dram_;
}

void AddrTransController::MigratePages(Profiler& pf, double dr, double wr) {
  assert(!in_checkpointing());

  LoanRevoker loan_revoker(this, pf);
  att_.visitQueue(ATTEntry::LOAN, &loan_revoker);
  assert(att_.isEmpty(ATTEntry::LOAN));

  migrator_.InputBlocks(att_.collectEntries());

  NVMPageStats n;
  DRAMPageStats d;
  bool d_ready = false;
  while (migrator_.ExtractNVMPage(n, pf)) {
    DPRINTF(Migration, "Extract NVM page DR=%f/%f, PTT #=%d/%d\n",
        n.dirty_ratio, dr, migrator_.num_entries(), migrator_.ptt_capacity());
    if (n.dirty_ratio < dr) break;
    // Find a DRAM page for exchange
    if (migrator_.num_entries() == migrator_.ptt_capacity()) {
      if (!migrator_.ExtractDRAMPage(d, Profiler::Overlap)) return;
      DPRINTF(Migration, "\tExtract DRAM page WR=%f\n", d.write_ratio);
      if (d.write_ratio > 0) {
        d_ready = true;
        break;
      }
      pf.setIgnoreLatency();
      MigrateDRAM(d, pf);
      pf.clearIgnoreLatency();
    }
    MigrateNVM(n, pf);
  }
  if ((!d_ready && !migrator_.ExtractDRAMPage(d, pf))) return;
  int limit = migrator_.ptt_capacity() - migrator_.ptt_length();
  do {
    DPRINTF(Migration, "Extract DRAM page WR=%f, S=%d\n",
        d.write_ratio, d.state);
    if (d.write_ratio == 0) continue;
    if (d.write_ratio > wr) break;
    pf.setIgnoreLatency();
    MigrateDRAM(d, pf);
    pf.clearIgnoreLatency();
    ++pages_to_nvm_; // excluding clean DRAM pages exchanged with NVM pages
    --limit;
  } while (limit && migrator_.ExtractDRAMPage(d,
      d.write_ratio == 0 ? Profiler::Null : pf));
}

void AddrTransController::BeginCheckpointing(Profiler& pf) {
  assert(!in_checkpointing());

  // ATT flush
  DirtyCleaner att_cleaner(this, pf);
  att_.visitQueue(ATTEntry::DIRTY, &att_cleaner);
  assert(att_.getLength(ATTEntry::CLEAN) +
      att_.getLength(ATTEntry::FREE) == att_.length());
  pf.addTableOp(); // assumed in parallel

  in_checkpointing_ = true;

  att_.clearStats(pf);
  migrator_.Clear(pf); // page write-back bytes
}

void AddrTransController::FinishCheckpointing() {
  assert(in_checkpointing());
  nvm_buffer_.clearBackup(Profiler::Null); //TODO
  in_checkpointing_ = false;
  // mem_store_->OnEpochEnd();
}

int AddrTransController::insert(Addr phy_addr, Addr hw_addr,
    ATTEntry::State state, bool move_data, Profiler& pf) {
  assert(state == ATTEntry::DIRTY || state == ATTEntry::PRE_DIRTY
      || state == ATTEntry::LOAN);

  const Tag phy_tag = att_.toTag(phy_addr);
  if (move_data) {
    CopyBlockIntra(hw_addr, att_.toAddr(phy_tag), pf);
  }
  return att_.insert(phy_tag, hw_addr, state, pf);
}

void AddrTransController::HideClean(int index, bool move_data, Profiler& pf) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.at(index);
  assert(entry.state == ATTEntry::CLEAN);

  const Addr phy_addr = att_.toAddr(entry.phy_tag);

  if (move_data) {
#ifdef MEMCK
    assert(!IsDRAM(phy_addr, Profiler::Null));
#endif
    CopyBlockIntra(phy_addr, entry.hw_addr, pf);
  }
  nvm_buffer_.backupSlot(entry.hw_addr, VersionBuffer::SHORT, pf);
  att_.reset(index, phy_addr, ATTEntry::HIDDEN, Profiler::Overlap);
}

Addr AddrTransController::ResetClean(int index, bool move_data, Profiler& pf) {
  assert(in_checkpointing());
  const ATTEntry& entry = att_.at(index);
  assert(entry.state == ATTEntry::CLEAN);

  const Addr hw_addr = dram_buffer_.allocSlot(pf);
  if (move_data) {
    CopyBlockInter(hw_addr, entry.hw_addr, pf);
  }
  nvm_buffer_.backupSlot(entry.hw_addr, VersionBuffer::LONG, pf);
  att_.reset(index, hw_addr, ATTEntry::PRE_HIDDEN, Profiler::Overlap);
  return hw_addr;
}

void AddrTransController::FreeClean(int index, Profiler& pf) {
  const ATTEntry& entry = att_.at(index);
  assert(entry.state == ATTEntry::CLEAN);

  Addr phy_addr = att_.toAddr(entry.phy_tag);
 
  if (in_checkpointing()) {
    SwapBlock(phy_addr, entry.hw_addr, pf);
  } else { // in running
#ifdef MEMCK
    assert(!IsDRAM(phy_addr, Profiler::Null));
#endif
    CopyBlockIntra(phy_addr, entry.hw_addr, pf);
  }
  nvm_buffer_.backupSlot(entry.hw_addr,
      (VersionBuffer::State)in_checkpointing(), pf);
  att_.shiftState(index, ATTEntry::FREE, Profiler::Overlap);
}

Addr AddrTransController::DirtyStained(int index, bool move_data,
    Profiler& pf) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.at(index);
  assert(entry.state == ATTEntry::PRE_DIRTY);

  const Addr hw_addr = nvm_buffer_.allocSlot(pf);
  if (move_data) {
    CopyBlockInter(hw_addr, entry.hw_addr, pf);
  }
  dram_buffer_.freeSlot(entry.hw_addr, VersionBuffer::IN_USE, pf);
  att_.reset(index, hw_addr, ATTEntry::DIRTY, Profiler::Overlap);
  return hw_addr;
}

void AddrTransController::FreeLoan(int index, bool move_data, Profiler& pf) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.at(index);
  assert(entry.state == ATTEntry::LOAN);

  const Addr phy_addr = att_.toAddr(entry.phy_tag);

  if (move_data) {
    assert(IsDRAM(phy_addr, Profiler::Null));
    CopyBlockIntra(phy_addr, entry.hw_addr, pf);
  }
  dram_buffer_.freeSlot(entry.hw_addr, VersionBuffer::IN_USE, pf);
  att_.shiftState(index, ATTEntry::FREE, Profiler::Overlap);
}

void AddrTransController::HideTemp(int index, bool move_data, Profiler& pf) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.at(index);
  assert(entry.state == ATTEntry::PRE_HIDDEN);

  const Addr phy_addr = att_.toAddr(entry.phy_tag);
  if (move_data) {
    assert(!IsDRAM(phy_addr, Profiler::Null));
    CopyBlockInter(phy_addr, entry.hw_addr, pf);
  }
  dram_buffer_.freeSlot(entry.hw_addr, VersionBuffer::IN_USE, pf);
  att_.reset(index, phy_addr, ATTEntry::HIDDEN, Profiler::Overlap);
}

void AddrTransController::Discard(int index, VersionBuffer& vb, Profiler& pf) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.at(index);
  vb.freeSlot(entry.hw_addr, VersionBuffer::IN_USE, Profiler::Overlap);
  att_.shiftState(index, ATTEntry::FREE, pf);
}

#ifdef MEMCK
  pair<AddrInfo, AddrInfo> AddrTransController::GetAddrInfo(Addr phy_addr) {
    pair<AddrInfo, AddrInfo> info;

    int index = att_.lookup(phy_addr, Profiler::Null);
    if (index != -EINVAL) {
      const ATTEntry& entry = att_.at(index);
      info.first = { att_.toAddr(entry.phy_tag), entry.hw_addr,
          entry.StateString() };
    } else {
      info.first = { 0, 0, NULL };
    }

    PTTEntry* p = migrator_.lookupPage(phy_addr, Profiler::Null);
    if (p) {
      info.second = { migrator_.PageAlign(phy_addr), p->hw_addr,
          p->StateString() };
    } else {
      info.second = { 0, 0, NULL };
    }

    return info;
  }
#endif
