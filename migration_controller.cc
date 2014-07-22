// migration_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "migration_controller.h"

using namespace std;

void MigrationController::InputBlocks(
    const vector<ATTEntry>& blocks) {
  assert(nvm_pages_.empty());
  for (vector<ATTEntry>::const_iterator it = blocks.begin();
      it != blocks.end(); ++it) {
    if (it->state == ATTEntry::CLEAN || it->state == ATTEntry::FREE) continue;
    uint64_t block_addr = it->phy_tag << block_bits_;
    NVMPage& p = nvm_pages_[PageAlign(block_addr)];
    p.epoch_reads += it->epoch_reads;
    p.epoch_writes += it->epoch_writes;
    total_writes_ += it->epoch_writes;
    if (it->epoch_writes) p.blocks.insert(block_addr);
  }
}

bool MigrationController::ExtractNVMPage(PageStats& stats,
    Profiler& profiler) {
  if (nvm_heap_.empty()) {
    double total_blocks = 1 << (page_bits_ - block_bits_);
    for (unordered_map<uint64_t, NVMPage>::iterator it = nvm_pages_.begin();
        it != nvm_pages_.end(); ++it) {
      double dr = it->second.blocks.size() / total_blocks;
      double wr = it->second.epoch_writes / total_blocks;
      nvm_heap_.push_back({it->first, PTTEntry::DIRTY_STATIC, dr, wr});
    }
    make_heap(nvm_heap_.begin(), nvm_heap_.end(), nvm_comp_);
  }
  profiler.AddTableOp();

  if (nvm_heap_.empty()) return false;

  pop_heap(nvm_heap_.begin(), nvm_heap_.end(), nvm_comp_);
  stats = nvm_heap_.back();
  nvm_heap_.pop_back();
  return true;
}

bool MigrationController::ExtractDRAMPage(PageStats& stats,
    Profiler& profiler) {
  if (dram_heap_.empty()) {
    double total_blocks = 1 << (page_bits_ - block_bits_);
    for (unordered_map<uint64_t, PTTEntry>::iterator it = entries_.begin();
         it != entries_.end(); ++it) {
      double wr = it->second.epoch_writes / total_blocks;
      dram_heap_.push_back({it->first, it->second.state, 0, wr});
    }
    make_heap(dram_heap_.begin(), dram_heap_.end(), dram_comp_);
  }
  profiler.AddTableOp();

  if (dram_heap_.empty()) return false;

  pop_heap(dram_heap_.begin(), dram_heap_.end(), dram_comp_);
  stats = dram_heap_.back();
  dram_heap_.pop_back();
  return true;
}

void MigrationController::Clear(Profiler& profiler) {
  int dirts = 0;
  for (PTTEntryPtr it = entries_.begin(); it != entries_.end(); ++it) {
    it->second.epoch_reads = 0;
    it->second.epoch_writes = 0;
    if (it->second.state == PTTEntry::DIRTY_DIRECT) {
      ShiftState(it, PTTEntry::CLEAN_DIRECT, Profiler::Overlap);
      ++dirts;
    } else if (it->second.state == PTTEntry::CLEAN_STATIC) {
      ShiftState(it, PTTEntry::CLEAN_STATIC, Profiler::Overlap);
      ++dirts;
    }
  }
  profiler.AddTableOp();

  assert(dirts == dirty_pages_);
  profiler.AddPageMoveInter(dirty_pages_); // epoch write-backs

  nvm_pages_.clear();
  dram_heap_.clear();
  nvm_heap_.clear();
}
