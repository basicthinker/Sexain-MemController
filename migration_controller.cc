// migration_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "migration_controller.h"

using namespace std;

void MigrationController::InputBlocks(
    const vector<ATTEntry>& blocks) {
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
    total_writes_ += it->epoch_writes;
    if (it->epoch_writes) {
      p.blocks.insert(block_addr);
      assert(p.blocks.size() <= page_blocks_);
    }
  }
}

bool MigrationController::ExtractNVMPage(NVMPageStats& stats,
    Profiler& profiler) {
  if (nvm_heap_.empty()) {
    for (unordered_map<uint64_t, NVMPage>::iterator it = nvm_pages_.begin();
        it != nvm_pages_.end(); ++it) {
      double dr = it->second.blocks.size() / page_blocks_;
      double wr = it->second.epoch_writes / page_blocks_;
      nvm_heap_.push_back({it->first, dr, wr});
    }
    make_heap(nvm_heap_.begin(), nvm_heap_.end());
  }
  profiler.AddTableOp();

  if (nvm_heap_.empty()) return false;

  pop_heap(nvm_heap_.begin(), nvm_heap_.end());
  stats = nvm_heap_.back();
  nvm_heap_.pop_back();
  return true;
}

bool MigrationController::ExtractDRAMPage(DRAMPageStats& stats,
    Profiler& profiler) {
  if (dram_heap_.empty()) {
    for (unordered_map<uint64_t, PTTEntry>::iterator it = entries_.begin();
         it != entries_.end(); ++it) {
      double wr = it->second.epoch_writes / page_blocks_;
      dram_heap_.push_back({it->first, it->second.state, wr});
    }
    make_heap(dram_heap_.begin(), dram_heap_.end());
  }
  profiler.AddTableOp();

  if (dram_heap_.empty()) return false;

  pop_heap(dram_heap_.begin(), dram_heap_.end());
  stats = dram_heap_.back();
  dram_heap_.pop_back();
  return true;
}

void MigrationController::Clear(Profiler& profiler) {
  int dirts = 0;
  for (PTTEntryIterator it = entries_.begin(); it != entries_.end(); ++it) {
    it->second.epoch_reads = 0;
    it->second.epoch_writes = 0;
    if (it->second.state == PTTEntry::DIRTY_DIRECT) {
      ShiftState(it->second, PTTEntry::CLEAN_DIRECT, Profiler::Overlap);
      ++dirts;
    } else if (it->second.state == PTTEntry::CLEAN_STATIC) {
      ShiftState(it->second, PTTEntry::CLEAN_STATIC, Profiler::Overlap);
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