// migration_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "migration_controller.h"

using namespace std;

const char* PTTEntry::state_strings[] = {
    "CLEAN_DIRECT", "CLEAN_STATIC", "DIRTY_DIRECT", "DIRTY_STATIC"
};

void MigrationController::FillNVMPageHeap() {
  for (unordered_map<Addr, NVMPage>::iterator it = nvm_pages_.begin();
      it != nvm_pages_.end(); ++it) {
    double dr = it->second.blocks.size() / (double)page_blocks_;
    double wr = it->second.epoch_writes / (double)page_blocks_;
    nvm_heap_.push_back({it->first, dr, wr});

    total_nvm_writes_ += it->second.epoch_writes;
    dirty_nvm_blocks_ += it->second.blocks.size();
  }
  make_heap(nvm_heap_.begin(), nvm_heap_.end());
  nvm_heap_filled_ = true;
}

void MigrationController::FillDRAMPageHeap() {
  int dirts = 0;
  for (unordered_map<Addr, PTTEntry>::iterator it = entries_.begin();
       it != entries_.end(); ++it) {
    double wr = it->second.epoch_writes / (double)page_blocks_;
    dram_heap_.push_back({it->first, it->second.state, wr});

    total_dram_writes_ += it->second.epoch_writes;
    dirts += (it->second.epoch_writes ? 1 : 0);
  }
  assert(dirts == dirty_entries_);
  dirty_dram_pages_ += dirty_entries_;

  make_heap(dram_heap_.begin(), dram_heap_.end());
  dram_heap_filled_ = true;
}

void MigrationController::InputBlocks(const vector<ATTEntry>& blocks) {
  assert(nvm_pages_.empty());
  for (vector<ATTEntry>::const_iterator it = blocks.begin();
      it != blocks.end(); ++it) {
    if (it->state == ATTEntry::CLEAN || it->state == ATTEntry::FREE) {
      assert(it->epoch_writes == 0);
      continue;
    }
    Addr block_addr = it->phy_tag << block_bits_;
    NVMPage& p = nvm_pages_[PageAlign(block_addr)];
    p.epoch_reads += it->epoch_reads;
    p.epoch_writes += it->epoch_writes;

    if (it->epoch_writes) {
      p.blocks.insert(block_addr);
      assert(p.blocks.size() <= page_blocks_);
    }
  }
  dirty_nvm_pages_ += nvm_pages_.size();

  FillNVMPageHeap();
  FillDRAMPageHeap();
}

bool MigrationController::ExtractNVMPage(NVMPageStats& stats, Profiler& pf) {
  assert(nvm_heap_filled_);

  if (nvm_heap_.empty()) return false;

  stats = nvm_heap_.front();
  pop_heap(nvm_heap_.begin(), nvm_heap_.end());
  nvm_heap_.pop_back();

  pf.AddTableOp();
  return true;
}

bool MigrationController::ExtractDRAMPage(DRAMPageStats& stats, Profiler& pf) {
  assert(dram_heap_filled_);

  if (dram_heap_.empty()) return false;

  stats = dram_heap_.front();
  pop_heap(dram_heap_.begin(), dram_heap_.end());
  dram_heap_.pop_back();

  pf.AddTableOp();
  return true;
}

void MigrationController::AddToBlockList(Addr page, std::list<Addr>* list) {
  assert((page & page_mask_) == 0 && list);
  Addr next_page = page + page_size();
  for (Addr a = page; a < next_page; a += (1 << block_bits_)) {
    list->push_back(a);
  }
}

void MigrationController::Clear(Profiler& pf, std::list<Addr>* ckpt_blocks) {
  pf.AddPageMoveInter(dirty_entries_); // epoch write-backs
  for (PTTEntryIterator it = entries_.begin(); it != entries_.end(); ++it) {
    it->second.epoch_reads = 0;
    it->second.epoch_writes = 0;
    if (it->second.state == PTTEntry::DIRTY_DIRECT) {
      ShiftState(it, PTTEntry::CLEAN_DIRECT, pf);
      --dirty_entries_;
      AddToBlockList(it->second.mach_base, ckpt_blocks);
    } else if (it->second.state == PTTEntry::DIRTY_STATIC) {
      ShiftState(it, PTTEntry::CLEAN_STATIC, pf);
      --dirty_entries_;
      AddToBlockList(it->second.mach_base, ckpt_blocks);
    }
  }
  assert(dirty_entries_ == 0);

  nvm_pages_.clear();
  dram_heap_.clear();
  nvm_heap_.clear();
  dram_heap_filled_ = false;
  nvm_heap_filled_ = false;
}
