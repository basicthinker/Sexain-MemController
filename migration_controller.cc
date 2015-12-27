// migration_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "migration_controller.h"

using namespace std;
using namespace thynvm;

const char* PTTEntry::state_strings[] = {
    "CLEAN_DIRECT", "CLEAN_STATIC", "DIRTY_DIRECT", "DIRTY_STATIC"
};

void MigrationController::FillNVMPageHeap() {
  for (unordered_map<uint64_t, NVMPage>::iterator it = nvm_pages_.begin();
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
  for (unordered_map<uint64_t, PTTEntry>::iterator it = entries_.begin();
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
    uint64_t block_addr = it->phy_tag << block_bits_;
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

  pf.addTableOp();
  return true;
}

bool MigrationController::ExtractDRAMPage(DRAMPageStats& stats, Profiler& pf) {
  assert(dram_heap_filled_);

  if (dram_heap_.empty()) return false;

  stats = dram_heap_.front();
  pop_heap(dram_heap_.begin(), dram_heap_.end());
  dram_heap_.pop_back();

  pf.addTableOp();
  return true;
}

void MigrationController::Clear(Profiler& pf) {
  pf.addPageInterChannel(dirty_entries_); // epoch write-backs
  for (PTTEntryIterator it = entries_.begin(); it != entries_.end(); ++it) {
    it->second.epoch_reads = 0;
    it->second.epoch_writes = 0;
    if (it->second.state == PTTEntry::DIRTY_DIRECT) {
      shiftState(it->second, PTTEntry::CLEAN_DIRECT, pf);
      --dirty_entries_;
    } else if (it->second.state == PTTEntry::DIRTY_STATIC) {
      shiftState(it->second, PTTEntry::CLEAN_STATIC, pf);
      --dirty_entries_;
    }
  }
  assert(dirty_entries_ == 0);

  nvm_pages_.clear();
  dram_heap_.clear();
  nvm_heap_.clear();
  dram_heap_filled_ = false;
  nvm_heap_filled_ = false;
}
