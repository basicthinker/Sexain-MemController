// migration_controller.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_MIGRATION_CONTROLLER_H_
#define SEXAIN_MIGRATION_CONTROLLER_H_

#include <cassert>
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>

#include "addr_trans_table.h"
#include "profiler.h"

struct PTTEntry {
  enum State {
    CLEAN_DIRECT = 0, ///< Sorted with precedence
    CLEAN_STATIC = 1,
    DIRTY_DIRECT,
    DIRTY_STATIC,
  };

  State state;
  int epoch_reads;
  int epoch_writes;
  uint64_t mach_base;

  PTTEntry() : epoch_reads(0), epoch_writes(0) { }
};

struct DRAMPageStats {
  uint64_t phy_addr;
  PTTEntry::State state;
  double write_ratio;
  bool operator<(const DRAMPageStats& p) {
    if (write_ratio == p.write_ratio) {
      return state > p.state;
    } else return write_ratio > p.write_ratio;
  }
};

struct NVMPageStats {
  uint64_t phy_addr;
  double dirty_ratio;
  double write_ratio;
  bool operator<(const NVMPageStats& p) {
    return dirty_ratio < p.dirty_ratio;
  }
};

class MigrationController {
 public:
  MigrationController(int block_bits, int page_bits, int ptt_limit);

  PTTEntry* LookupPage(uint64_t phy_addr, Profiler& profiler);
  bool Contains(uint64_t phy_addr, Profiler& profiler);
  void ShiftState(PTTEntry& entry, PTTEntry::State state, Profiler& profiler);
  void Free(uint64_t page_addr, Profiler& profiler);
  void Setup(uint64_t page_addr, PTTEntry::State state, Profiler& profiler);
  uint64_t Translate(uint64_t phy_addr, uint64_t page_base) const;
  void AddDRAMPageRead(PTTEntry& entry);
  void AddDRAMPageWrite(PTTEntry& entry);

  /// Calculate statistics over the blocks from ATT
  void InputBlocks(const std::vector<ATTEntry>& blocks);
  /// Next NVM pages with decreasing dirty ratio
  bool ExtractNVMPage(NVMPageStats& stats, Profiler& profiler);
  /// Next DRAM page with increasing dirty ratio
  bool ExtractDRAMPage(DRAMPageStats& stats, Profiler& profiler);
  /// Clear up all entries, heaps, epoch statistics, etc.
  void Clear(Profiler& profiler);

  int page_bits() const { return page_bits_; }
  int page_size() const { return 1 << page_bits_; }
  int page_blocks() const { return page_blocks_; }

  int total_nvm_writes() const { return total_nvm_writes_; }
  int total_dram_writes() const { return total_dram_writes_; }
  int dirty_nvm_blocks() const { return dirty_nvm_blocks_; }
  int dirty_nvm_pages() const { return dirty_nvm_pages_; }
  int dirty_dram_pages() const { return dirty_dram_pages_; }

 private:
  typedef std::unordered_map<uint64_t, PTTEntry>::iterator PTTEntryIterator;

  struct NVMPage {
    int epoch_reads;
    int epoch_writes;
    std::set<uint64_t> blocks;
  };

  uint64_t BlockAlign(uint64_t addr) { return addr & ~block_mask_; }
  uint64_t PageAlign(uint64_t addr) { return addr & ~page_mask_; }

  const int block_bits_;
  const uint64_t block_mask_;
  const int page_bits_;
  const uint64_t page_mask_;
  const int page_blocks_;
  const int ptt_limit_;

  int dirty_entries_; ///< Number of dirty pages each epoch

  int total_nvm_writes_; ///< Sum number of NVM writes, for verification
  int total_dram_writes_; ///< Sum number of DRAM writes, for verification
  int dirty_nvm_blocks_; ///< Sum number of dirty NVM blocks
  int dirty_nvm_pages_; ///< Sum number of dirty NVM pages
  int dirty_dram_pages_; ///< Sum number of dirty DRAM pages

  std::unordered_map<uint64_t, PTTEntry> entries_;
  std::unordered_map<uint64_t, NVMPage> nvm_pages_;
  std::vector<DRAMPageStats> dram_heap_;
  std::vector<NVMPageStats> nvm_heap_;
};

inline MigrationController::MigrationController(
    int block_bits, int page_bits, int ptt_limit) :

    block_bits_(block_bits), block_mask_((1 << block_bits) - 1),
    page_bits_(page_bits), page_mask_((1 << page_bits) - 1),
    page_blocks_(1 << (page_bits - block_bits)),
    ptt_limit_(ptt_limit), dirty_entries_(0),
    total_nvm_writes_(0), total_dram_writes_(0),
    dirty_nvm_blocks_(0), dirty_dram_pages_(0) {
}

inline PTTEntry* MigrationController::LookupPage(uint64_t phy_addr,
    Profiler& profiler) {
  profiler.AddTableOp();
  PTTEntryIterator it = entries_.find(PageAlign(phy_addr));
  if (it == entries_.end()) return NULL;
  return &it->second;
}

inline bool MigrationController::Contains(uint64_t phy_addr,
    Profiler& profiler) {
  profiler.AddTableOp();
  return entries_.find(PageAlign(phy_addr)) != entries_.end();
}

inline void MigrationController::AddDRAMPageRead(PTTEntry& entry) {
  ++entry.epoch_reads;
}

inline void MigrationController::AddDRAMPageWrite(PTTEntry& entry) {
  ++entry.epoch_writes;
}

inline void MigrationController::ShiftState(
    PTTEntry& entry, PTTEntry::State state, Profiler& profiler) {
  entry.state = state;
  if (state == PTTEntry::DIRTY_DIRECT || state == PTTEntry::DIRTY_STATIC) {
    ++dirty_entries_;
  }
  profiler.AddTableOp();
}

inline void MigrationController::Free(uint64_t page_addr, Profiler& profiler) {
  PTTEntry* page = LookupPage(page_addr, Profiler::Overlap);
  assert(page);
  if (page->state == PTTEntry::DIRTY_DIRECT ||
      page->state == PTTEntry::DIRTY_STATIC) {
    --dirty_entries_;
  }
  assert(entries_.erase(page_addr) == 1);
  profiler.AddTableOp();
}

inline void MigrationController::Setup(
    uint64_t page_addr, PTTEntry::State state, Profiler& profiler) {
  assert((page_addr & page_mask_) == 0);
  PTTEntry& entry = entries_[page_addr];
  entry.state = state;
  entry.mach_base = page_addr; // simulate direct/static page allocation
  if (state == PTTEntry::DIRTY_DIRECT || state == PTTEntry::DIRTY_STATIC) {
    ++dirty_entries_;
  }
  assert(entries_.size() <= ptt_limit_);
  profiler.AddTableOp();
}

inline uint64_t MigrationController::Translate(uint64_t phy_addr,
    uint64_t page_base) const {
  assert((page_base & page_mask_) == 0);
  return page_base + (phy_addr & page_mask_);
}

#endif // SEXAIN_MIGRATION_CONTROLLER_H_
