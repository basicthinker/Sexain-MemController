// addr_trans_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include <vector>
#include "addr_trans_controller.h"
#include "base/trace.hh"
#include "debug/Migration.hh"

using namespace std;

// Space partition (low -> high):
// phy_limit || NVM buffer || DRAM buffer
// (virtual) DRAM backup || DRAM cache
AddrTransController::AddrTransController(
    uint64_t phy_range, uint64_t dram_size,
    int att_len, int block_bits, int page_bits, MemStore* ms):

    att_(att_len, block_bits), nvm_buffer_(2 * att_len, block_bits),
    dram_buffer_(att_len, block_bits),
    migrator_(block_bits, page_bits, dram_size >> page_bits),
    phy_range_(phy_range), pages_to_dram_(0), pages_to_nvm_(0) {

  assert(phy_range >= dram_size);
  mem_store_ = ms;
  in_checkpointing_ = false;

  nvm_buffer_.set_addr_base(phy_range_);
  dram_buffer_.set_addr_base(nvm_buffer_.addr_base() + nvm_buffer_.Size());
}

Addr AddrTransController::LoadAddr(Addr phy_addr, Profiler& pf) {
  int index = att_.Lookup(att_.ToTag(phy_addr), pf);
  if (index != -EINVAL) {
    const ATTEntry& entry = att_.At(index);
    att_.AddBlockRead(index);
    Addr mach_addr = att_.Translate(phy_addr, entry.mach_base);
    bool is_dram = (entry.state == ATTEntry::TEMP ||
        entry.state == ATTEntry::LOAN ||
        entry.state == ATTEntry::STAINED);
    pf.AddLatency(mem_store_->GetReadLatency(mach_addr, is_dram, NULL));
    return mach_addr;
  } else {
    PTTEntry page = migrator_.LookupPage(phy_addr, pf);
    Addr mach_addr;
    if (page.index < 0) {
      mach_addr = phy_addr;
      pf.AddLatency(mem_store_->GetReadLatency(mach_addr, false, NULL));
    } else {
      migrator_.AddDRAMPageRead(page.mach_base);
      mach_addr = migrator_.Translate(phy_addr, page.mach_base);
      pf.AddLatency(mem_store_->GetReadLatency(mach_addr, true, &page));
    }
    return mach_addr;
  }
}

Addr AddrTransController::DRAMStore(Addr phy_addr, int size,
    const PTTEntry& page, Profiler& pf) {
  int index = att_.Lookup(att_.ToTag(phy_addr), pf);
  if (index != -EINVAL) { // found
    const ATTEntry& entry = att_.At(index);
    mem_store_->OnATTWriteHit(entry.state);
    if (in_checkpointing()) {
      pf.AddLatency(mem_store_->GetWriteLatency(phy_addr, true, NULL));
      return att_.Translate(phy_addr, entry.mach_base);
    } else {
      FreeLoan(index, !FullBlock(phy_addr, size), pf);
      pf.AddLatency(mem_store_->GetWriteLatency(phy_addr, true, &page));
      return phy_addr;
    }
  } else { // not found
    if (in_checkpointing()) {
      if (att_.IsEmpty(ATTEntry::FREE)) {
        assert(!att_.IsEmpty(ATTEntry::CLEAN));
        FreeClean(att_.GetFront(ATTEntry::CLEAN), pf);
        mem_store_->OnATTWriteMiss(ATTEntry::LOAN);
      } else {
        mem_store_->OnATTWriteHit(ATTEntry::LOAN);
      }
      const Addr mach_base = dram_buffer_.SlotAlloc(Profiler::Overlap);
      Setup(phy_addr, mach_base, ATTEntry::LOAN, !FullBlock(phy_addr, size), pf);
      Addr mach_addr = att_.Translate(phy_addr, mach_base);
      pf.AddLatency(mem_store_->GetWriteLatency(mach_addr, true, NULL));
      return mach_addr;
    } else { // in running
      mem_store_->ckDRAMWriteHit();
      pf.AddLatency(mem_store_->GetWriteLatency(phy_addr, true, &page));
      return phy_addr;
    }
  }
}

Addr AddrTransController::NVMStore(Addr phy_addr, int size, Profiler& pf) {
  const Tag phy_tag = att_.ToTag(phy_addr);
  int index = att_.Lookup(phy_tag, pf);
  Addr mach_addr;
  if (!in_checkpointing()) {
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.At(index);
      mem_store_->OnATTWriteHit(entry.state);
      switch(entry.state) {
      case ATTEntry::TEMP:
        HideTemp(index, !FullBlock(phy_addr, size), pf);
      case ATTEntry::DIRTY:
      case ATTEntry::HIDDEN:
        mach_addr = att_.Translate(phy_addr, entry.mach_base);
        break;
      case ATTEntry::STAINED:
        mach_addr = att_.Translate(phy_addr,
            DirtyStained(index, !FullBlock(phy_addr, size), pf));
        break;
      default:
        HideClean(index, !FullBlock(phy_addr, size), pf);
        mach_addr = phy_addr;
        break;
      }
    } else { // not found
      if (att_.IsEmpty(ATTEntry::FREE)) {
        if (!att_.IsEmpty(ATTEntry::LOAN)) {
          int li = att_.GetFront(ATTEntry::LOAN);
          FreeLoan(li, true, pf);
        } else {
          assert(!att_.IsEmpty(ATTEntry::CLEAN));
          int ci = att_.GetFront(ATTEntry::CLEAN);
          FreeClean(ci, pf);
        }
        mem_store_->OnATTWriteMiss(ATTEntry::DIRTY);
      } else {
        mem_store_->OnATTWriteHit(ATTEntry::DIRTY);
      }
      Addr mach_base = nvm_buffer_.SlotAlloc(Profiler::Overlap);
      index = Setup(phy_addr, mach_base, ATTEntry::DIRTY,
          !FullBlock(phy_addr, size), pf);
      mach_addr = att_.Translate(phy_addr, mach_base);
    }
    pf.AddLatency(mem_store_->GetWriteLatency(mach_addr, false, NULL));
    att_.AddBlockWrite(index);
    return mach_addr;
  } else { // in checkpointing
    if (index != -EINVAL) { // found
      const ATTEntry& entry = att_.At(index);
      mem_store_->OnATTWriteHit(entry.state);
      if (entry.state == ATTEntry::TEMP || entry.state == ATTEntry::STAINED) {
        mach_addr = att_.Translate(phy_addr, entry.mach_base);
      } else {
        Addr mach_base = ResetClean(index, !FullBlock(phy_addr, size), pf);
        mach_addr = att_.Translate(phy_addr, mach_base);
      }
    } else { // not found
      if (att_.IsEmpty(ATTEntry::FREE)) {
        assert(!att_.IsEmpty(ATTEntry::CLEAN));
        FreeClean(att_.GetFront(ATTEntry::CLEAN), pf);
        mem_store_->OnATTWriteMiss(ATTEntry::STAINED);
      } else {
        mem_store_->OnATTWriteHit(ATTEntry::STAINED);
      }
      Addr mach_base = dram_buffer_.SlotAlloc(Profiler::Overlap);
      index = Setup(phy_addr, mach_base, ATTEntry::STAINED,
          !FullBlock(phy_addr, size), pf);
      mach_addr = att_.Translate(phy_addr, mach_base);
    }
    pf.AddLatency(mem_store_->GetWriteLatency(mach_addr, true, NULL));
    att_.AddBlockWrite(index);
    return mach_addr;
  }
}

Control AddrTransController::Probe(Addr phy_addr) {
  if (migrator_.Contains(phy_addr, Profiler::Null)) { // DRAM
    if (in_checkpointing()) {
      if (!att_.Contains(phy_addr, Profiler::Null) &&
          att_.IsEmpty(ATTEntry::FREE) && att_.IsEmpty(ATTEntry::CLEAN)) {
        return WAIT_CKPT;
      }
    }
  } else if (!att_.Contains(phy_addr, Profiler::Null)) { // NVM
    if (in_checkpointing()) {
      if (att_.IsEmpty(ATTEntry::FREE) && att_.IsEmpty(ATTEntry::CLEAN)) {
        return WAIT_CKPT;
      }
    } else if (att_.GetLength(ATTEntry::DIRTY) == att_.length() ||
         migrator_.num_dirty_entries() >= migrator_.ptt_length() ) {
      return NEW_EPOCH;
    }
  }
  return REG_WRITE;
}

Addr AddrTransController::StoreAddr(Addr phy_addr, int size, Profiler& pf) {
  assert(CheckValid(phy_addr, size) && phy_addr < phy_range_);
  PTTEntry page = migrator_.LookupPage(phy_addr, pf);
  if (page.index < 0) {
    mem_store_->statsNVMWrites();
    Addr mach_addr = NVMStore(phy_addr, size, pf);
    return mach_addr;
  } else {
    migrator_.AddDRAMPageWrite(page.mach_base);
    if (page.state == PTTEntry::CLEAN_STATIC) {
      migrator_.ShiftState(page.mach_base, PTTEntry::DIRTY_DIRECT, pf);
      page.state = PTTEntry::DIRTY_DIRECT;
    } else if (page.state == PTTEntry::CLEAN_DIRECT) {
      migrator_.ShiftState(page.mach_base, PTTEntry::DIRTY_STATIC, pf);
      page.state = PTTEntry::DIRTY_STATIC;
    }
    mem_store_->statsDRAMWrites();
    Addr mach_addr = DRAMStore(phy_addr, size, page, pf);
    return mach_addr;
  }
}

void AddrTransController::DirtyCleaner::Visit(int i) {
  const ATTEntry& entry = atc_->att_.At(i);
  if (entry.state == ATTEntry::STAINED) {
    atc_->DirtyStained(i, true, pf_, ckpt_blocks_);
  } else if (entry.state == ATTEntry::TEMP) {
    atc_->HideTemp(i, true, pf_, ckpt_blocks_);
  }

  if (entry.state == ATTEntry::DIRTY) {
    atc_->att_.ShiftState(i, ATTEntry::CLEAN, Profiler::Overlap);
  } else if (entry.state == ATTEntry::HIDDEN) {
    atc_->att_.ShiftState(i, ATTEntry::FREE, Profiler::Overlap);
  }
}

void AddrTransController::LoanRevoker::Visit(int i) {
  const ATTEntry& entry = atc_->att_.At(i);
  assert(entry.state == ATTEntry::LOAN);
  atc_->FreeLoan(i, true, pf_, ckpt_blocks_);
}

void AddrTransController::MigrateDRAM(const DRAMPageStats& stats,
    list<Addr>& ckpt_queue, Profiler& pf) {
  DPRINTF(Migration, "Migrate DRAM page WR=%f, from %s.\n",
      stats.write_ratio, PTTEntry::state_strings[stats.state]);
  if (stats.state == PTTEntry::CLEAN_STATIC) {
    pf.AddPageMoveIntra();
    migrator_.AddToBlockList(stats.phy_addr, &ckpt_queue);
  } else if (stats.state == PTTEntry::DIRTY_DIRECT) {
    pf.AddPageMoveInter(); // for write back
    migrator_.AddToBlockList(stats.phy_addr, &ckpt_queue);
  } else if (stats.state == PTTEntry::DIRTY_STATIC) {
    pf.AddPageMoveIntra();
    pf.AddPageMoveInter();
    migrator_.AddToBlockList(stats.phy_addr, &ckpt_queue);
    migrator_.AddToBlockList(stats.phy_addr, &ckpt_queue);
  }
#ifdef MEMCK
  Tag tag = att_.ToTag(stats.phy_addr);
  for (int i = 0; i < migrator_.page_blocks(); ++i) {
    assert(!att_.Contains(tag + i, Profiler::Null));
  }
#endif
  migrator_.Free(stats.phy_addr, pf);
}

void AddrTransController::MigrateNVM(const NVMPageStats& stats,
    list<Addr>& ckpt_queue, Profiler& pf) {
  Tag phy_tag = att_.ToTag(stats.phy_addr);
  Tag next_page = att_.ToTag(stats.phy_addr + migrator_.page_size());
  assert(next_page - phy_tag == migrator_.page_blocks());

  for (Tag tag = phy_tag; tag < next_page; ++tag) {
    int index = att_.Lookup(tag, pf);
    if (index == -EINVAL) {
      pf.AddBlockMoveInter(); // for copying data to DRAM
      continue;
    }
    const ATTEntry& entry = att_.At(index);

    pf.set_ignore_latency();
    switch (entry.state) {
      case ATTEntry::CLEAN:
      case ATTEntry::DIRTY:
        pf.AddBlockMoveInter(); // for copying data to DRAM
        CopyBlockIntra(att_.ToAddr(entry.phy_tag), entry.mach_base, pf);
        ckpt_queue.push_back(att_.ToAddr(entry.phy_tag));
        Discard(index, nvm_buffer_, pf);
        break;
      case ATTEntry::STAINED:
      case ATTEntry::TEMP:
        pf.AddBlockMoveIntra(); // for copying data to DRAM
        CopyBlockInter(att_.ToAddr(entry.phy_tag), entry.mach_base, pf);
        ckpt_queue.push_back(att_.ToAddr(entry.phy_tag));
        Discard(index, dram_buffer_, pf);
        break;
      case ATTEntry::LOAN:
        assert(entry.state != ATTEntry::LOAN);
      case ATTEntry::FREE:
        assert(entry.state != ATTEntry::FREE);
      case ATTEntry::HIDDEN:
        pf.AddBlockMoveInter(); // for copying data to DRAM
        att_.ShiftState(index, ATTEntry::FREE, pf);
        break;
    }
    pf.clear_ignore_latency();
  }
  if (stats.dirty_ratio > 0.5) {
    migrator_.Setup(stats.phy_addr, PTTEntry::CLEAN_STATIC, pf);
    DPRINTF(Migration, "Migrate NVM page to CLEAN_STATIC.\n");
  } else {
    migrator_.Setup(stats.phy_addr, PTTEntry::CLEAN_DIRECT, pf);
    DPRINTF(Migration, "Migrate NVM page to CLEAN_DIRECT.\n");
  }
  migrator_.AddToBlockList(stats.phy_addr, &ckpt_queue);
  ++pages_to_dram_;
}

void AddrTransController::MigratePages(list<Addr>& ckpt_queue, Profiler& pf,
    double dr, double wr) {
  assert(!in_checkpointing());

  LoanRevoker loan_revoker(this, pf, &ckpt_queue);
  att_.VisitQueue(ATTEntry::LOAN, &loan_revoker);
  assert(att_.IsEmpty(ATTEntry::LOAN));

  migrator_.InputBlocks(att_.entries());

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
      pf.set_ignore_latency();
      MigrateDRAM(d, ckpt_queue, pf);
      pf.clear_ignore_latency();
    }
    MigrateNVM(n, ckpt_queue, pf);
  }
  if ((!d_ready && !migrator_.ExtractDRAMPage(d, pf))) return;
  int limit = migrator_.ptt_capacity() - migrator_.ptt_length();
  do {
    DPRINTF(Migration, "Extract DRAM page WR=%f, S=%d\n",
        d.write_ratio, d.state);
    if (d.write_ratio == 0) continue;
    if (d.write_ratio > wr) break;
    pf.set_ignore_latency();
    MigrateDRAM(d, ckpt_queue, pf);
    pf.clear_ignore_latency();
    ++pages_to_nvm_; // excluding clean DRAM pages exchanged with NVM pages
    --limit;
  } while (limit && migrator_.ExtractDRAMPage(d,
      d.write_ratio == 0 ? Profiler::Null : pf));
}

void AddrTransController::BeginCheckpointing(
    list<Addr>& ckpt_queue, Profiler& pf) {
  assert(!in_checkpointing());

  // ATT flush
  DirtyCleaner att_cleaner(this, pf, &ckpt_queue);
  att_.VisitQueue(ATTEntry::DIRTY, &att_cleaner);
  assert(att_.GetLength(ATTEntry::CLEAN) +
      att_.GetLength(ATTEntry::FREE) == att_.length());
  pf.AddTableOp(); // assumed in parallel

  in_checkpointing_ = true;

  att_.ClearStats(pf);
  migrator_.Clear(pf, &ckpt_queue); // page write-back
}

void AddrTransController::FinishCheckpointing(list<Addr>& ckpt_queue) {
  assert(in_checkpointing());
  assert(ckpt_queue.empty());
  nvm_buffer_.ClearBackup(Profiler::Null); //TODO
  in_checkpointing_ = false;
  mem_store_->OnEpochEnd();
}

int AddrTransController::Setup(Addr phy_addr, Addr mach_base,
    ATTEntry::State state, bool move_data, Profiler& pf) {
  assert(state == ATTEntry::DIRTY || state == ATTEntry::STAINED
      || state == ATTEntry::LOAN);

  const Tag phy_tag = att_.ToTag(phy_addr);
  if (move_data) {
    CopyBlockIntra(mach_base, att_.ToAddr(phy_tag), pf);
  }
  return att_.Setup(phy_tag, mach_base, state, pf);
}

void AddrTransController::HideClean(int index, bool move_data, Profiler& pf) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::CLEAN);

  const Addr phy_addr = att_.ToAddr(entry.phy_tag);

  if (move_data) {
#ifdef MEMCK
    assert(!IsDRAM(phy_addr, Profiler::Null));
#endif
    CopyBlockIntra(phy_addr, entry.mach_base, pf);
  }
  nvm_buffer_.SlotBackup(entry.mach_base, VersionBuffer::BACKUP0, pf);
  att_.Reset(index, phy_addr, ATTEntry::HIDDEN, Profiler::Overlap);
}

Addr AddrTransController::ResetClean(int index, bool move_data, Profiler& pf) {
  assert(in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::CLEAN);

  const Addr mach_base = dram_buffer_.SlotAlloc(pf);
  if (move_data) {
    CopyBlockInter(mach_base, entry.mach_base, pf);
  }
  nvm_buffer_.SlotBackup(entry.mach_base, VersionBuffer::BACKUP1, pf);
  att_.Reset(index, mach_base, ATTEntry::TEMP, Profiler::Overlap);
  return mach_base;
}

void AddrTransController::FreeClean(int index, Profiler& pf) {
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::CLEAN);

  Addr phy_addr = att_.ToAddr(entry.phy_tag);
 
  if (in_checkpointing()) {
    SwapBlock(phy_addr, entry.mach_base, pf);
  } else { // in running
#ifdef MEMCK
    assert(!IsDRAM(phy_addr, Profiler::Null));
#endif
    CopyBlockIntra(phy_addr, entry.mach_base, pf);
  }
  nvm_buffer_.SlotBackup(entry.mach_base,
      (VersionBuffer::State)in_checkpointing(), pf);
  att_.ShiftState(index, ATTEntry::FREE, Profiler::Overlap);
}

Addr AddrTransController::DirtyStained(int index, bool move_data,
    Profiler& pf, std::list<Addr>* ckpt_blocks) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::STAINED);

  const Addr mach_base = nvm_buffer_.SlotAlloc(pf);
  if (move_data) {
    CopyBlockInter(mach_base, entry.mach_base, pf);
    if (ckpt_blocks) ckpt_blocks->push_back(mach_base);
  }
  dram_buffer_.FreeSlot(entry.mach_base, VersionBuffer::IN_USE, pf);
  att_.Reset(index, mach_base, ATTEntry::DIRTY, Profiler::Overlap);
  return mach_base;
}

void AddrTransController::FreeLoan(int index, bool move_data,
    Profiler& pf, std::list<Addr>* ckpt_blocks) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::LOAN);

  const Addr phy_addr = att_.ToAddr(entry.phy_tag);

  if (move_data) {
    assert(IsDRAM(phy_addr, Profiler::Null));
    CopyBlockIntra(phy_addr, entry.mach_base, pf);
    if (ckpt_blocks) ckpt_blocks->push_back(phy_addr);
  }
  dram_buffer_.FreeSlot(entry.mach_base, VersionBuffer::IN_USE, pf);
  att_.ShiftState(index, ATTEntry::FREE, Profiler::Overlap);
}

void AddrTransController::HideTemp(int index, bool move_data,
    Profiler& pf, std::list<Addr>* ckpt_blocks) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  assert(entry.state == ATTEntry::TEMP);

  const Addr phy_addr = att_.ToAddr(entry.phy_tag);
  if (move_data) {
    assert(!IsDRAM(phy_addr, Profiler::Null));
    CopyBlockInter(phy_addr, entry.mach_base, pf);
    if (ckpt_blocks) ckpt_blocks->push_back(phy_addr);
  }
  dram_buffer_.FreeSlot(entry.mach_base, VersionBuffer::IN_USE, pf);
  att_.Reset(index, phy_addr, ATTEntry::HIDDEN, Profiler::Overlap);
}

void AddrTransController::Discard(int index, VersionBuffer& vb, Profiler& pf) {
  assert(!in_checkpointing());
  const ATTEntry& entry = att_.At(index);
  vb.FreeSlot(entry.mach_base, VersionBuffer::IN_USE, Profiler::Overlap);
  att_.ShiftState(index, ATTEntry::FREE, pf);
}

#ifdef MEMCK
  pair<AddrInfo, AddrInfo> AddrTransController::GetAddrInfo(Addr phy_addr) {
    pair<AddrInfo, AddrInfo> info;

    int index = att_.Lookup(phy_addr, Profiler::Null);
    if (index != -EINVAL) {
      const ATTEntry& entry = att_.At(index);
      info.first = { att_.ToAddr(entry.phy_tag), entry.mach_base,
          entry.StateString() };
    } else {
      info.first = { 0, 0, NULL };
    }

    PTTEntry* p = migrator_.LookupPage(phy_addr, Profiler::Null);
    if (p) {
      info.second = { migrator_.PageAlign(phy_addr), p->mach_base,
          p->StateString() };
    } else {
      info.second = { 0, 0, NULL };
    }

    return info;
  }
#endif
