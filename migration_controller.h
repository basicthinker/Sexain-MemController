// migration_controller.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_MIGRATION_CONTROLLER_H_
#define SEXAIN_MIGRATION_CONTROLLER_H_

#include <cassert>
#include <vector>
#include <unordered_map>
#include <set>
#include <list>
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
  int index;
  Addr mach_base;

  static const char* state_strings[];

  PTTEntry() : epoch_reads(0), epoch_writes(0), index(-EINVAL) { }

  const char* StateString() const {
    return state_strings[state];
  }
};

struct DRAMPageStats {
  Addr phy_addr;
  PTTEntry::State state;
  double write_ratio;

  bool operator<(const DRAMPageStats& p) {
    if (state == p.state) {
      return write_ratio > p.write_ratio;
    } else return state > p.state;
  }
};

struct NVMPageStats {
  Addr phy_addr;
  double dirty_ratio;
  double write_ratio;
  bool operator<(const NVMPageStats& p) {
    return dirty_ratio < p.dirty_ratio;
  }
};

class MigrationController {
 public:
  MigrationController(int block_bits, int page_bits, int ptt_length);

  PTTEntry LookupPage(Addr phy_addr, Profiler& profiler);
  bool Contains(Addr phy_addr, Profiler& profiler);
  void ShiftState(Addr phy_addr, PTTEntry::State state, Profiler& pf);
  void Free(Addr page_addr, Profiler& profiler);
  void Setup(Addr page_addr, PTTEntry::State state, Profiler& profiler);

  Addr Translate(Addr phy_addr, Addr page_base) const;
  void AddDRAMPageRead(Addr phy_addr);
  void AddDRAMPageWrite(Addr phy_addr);

  /// Calculate statistics over the blocks from ATT
  void InputBlocks(const std::vector<ATTEntry>& blocks);
  /// Next NVM pages with decreasing dirty ratio
  bool ExtractNVMPage(NVMPageStats& stats, Profiler& profiler);
  /// Next DRAM page with increasing dirty ratio
  bool ExtractDRAMPage(DRAMPageStats& stats, Profiler& profiler);
  /// Clear up all entries, heaps, epoch statistics, etc.
  void Clear(Profiler& profiler, std::list<Addr>* ckpt_blocks);

  void AddToBlockList(Addr page, std::list<Addr>* list);

  int page_bits() const { return page_bits_; }
  int page_size() const { return 1 << page_bits_; }
  int page_blocks() const { return page_blocks_; }
  Addr BlockAlign(Addr addr) { return addr & ~block_mask_; }
  Addr PageAlign(Addr addr) { return addr & ~page_mask_; }

  int ptt_length() const { return ptt_length_; }
  int ptt_capacity() const { return ptt_capacity_; }
  int num_entries() const { return entries_.size(); }
  int num_dirty_entries() const { return dirty_entries_; }

  uint64_t total_nvm_writes() const { return total_nvm_writes_; }
  uint64_t total_dram_writes() const { return total_dram_writes_; }
  uint64_t dirty_nvm_blocks() const { return dirty_nvm_blocks_; }
  uint64_t dirty_nvm_pages() const { return dirty_nvm_pages_; }
  uint64_t dirty_dram_pages() const { return dirty_dram_pages_; }

 private:
  typedef std::unordered_map<Addr, PTTEntry>::iterator PTTEntryIterator;

  struct NVMPage {
    int epoch_reads;
    int epoch_writes;
    std::set<Addr> blocks;
  };

  void FillNVMPageHeap();
  void FillDRAMPageHeap();
  void ShiftState(PTTEntryIterator it, PTTEntry::State state, Profiler& pf);

  const int block_bits_;
  const Addr block_mask_;
  const int page_bits_;
  const Addr page_mask_;
  const int page_blocks_;
  const int ptt_length_;
  const int ptt_capacity_;

  int dirty_entries_; ///< Number of dirty pages each epoch

  uint64_t total_nvm_writes_; ///< Sum number of NVM writes, for verification
  uint64_t total_dram_writes_; ///< Sum number of DRAM writes, for verification
  uint64_t dirty_nvm_blocks_; ///< Sum number of dirty NVM blocks
  uint64_t dirty_nvm_pages_; ///< Sum number of dirty NVM pages
  uint64_t dirty_dram_pages_; ///< Sum number of dirty DRAM pages

  std::vector<int> free_slots_;
  std::unordered_map<Addr, PTTEntry> entries_;
  std::unordered_map<Addr, NVMPage> nvm_pages_;
  std::vector<DRAMPageStats> dram_heap_;
  std::vector<NVMPageStats> nvm_heap_;
  bool dram_heap_filled_;
  bool nvm_heap_filled_;
};

inline MigrationController::MigrationController(
    int block_bits, int page_bits, int ptt_length) :

    block_bits_(block_bits), block_mask_((1 << block_bits) - 1),
    page_bits_(page_bits), page_mask_((1 << page_bits) - 1),
    page_blocks_(1 << (page_bits - block_bits)),
    ptt_length_(ptt_length), ptt_capacity_(ptt_length + (ptt_length >> 4)),
    dirty_entries_(0),
    total_nvm_writes_(0), total_dram_writes_(0),
    dirty_nvm_blocks_(0), dirty_nvm_pages_(0), dirty_dram_pages_(0),
    dram_heap_filled_(false), nvm_heap_filled_(false) {

  for (int i = 0; i < ptt_capacity_; ++i) {
    free_slots_.push_back(i);
  }
  assert(free_slots_.size() + entries_.size() == ptt_capacity_);
}

inline PTTEntry MigrationController::LookupPage(Addr phy_addr, Profiler& pf) {
  pf.AddTableOp();
  PTTEntryIterator it = entries_.find(PageAlign(phy_addr));
  if (it == entries_.end()) return PTTEntry();
  return it->second;
}

inline bool MigrationController::Contains(Addr phy_addr, Profiler& pf) {
  pf.AddTableOp();
  return entries_.find(PageAlign(phy_addr)) != entries_.end();
}

inline void MigrationController::AddDRAMPageRead(Addr phy_addr) {
  PTTEntryIterator it = entries_.find(PageAlign(phy_addr));
  assert(it != entries_.end());
  ++it->second.epoch_reads;
}

inline void MigrationController::AddDRAMPageWrite(Addr phy_addr) {
  PTTEntryIterator it = entries_.find(PageAlign(phy_addr));
  assert(it != entries_.end());
  ++it->second.epoch_writes;
}

inline void MigrationController::ShiftState(PTTEntryIterator it,
    PTTEntry::State state, Profiler& pf) {
  it->second.state = state;
  if (state == PTTEntry::DIRTY_DIRECT || state == PTTEntry::DIRTY_STATIC) {
    ++dirty_entries_;
  }
  pf.AddTableOp();
}

inline void MigrationController::ShiftState(Addr page_addr,
    PTTEntry::State state, Profiler& pf) {
  assert(PageAlign(page_addr) == page_addr);
  PTTEntryIterator it = entries_.find(page_addr);
  assert(it != entries_.end());
  ShiftState(it, state, pf);
}

inline void MigrationController::Free(Addr page_addr, Profiler& pf) {
  assert(PageAlign(page_addr) == page_addr);
  const PTTEntry entry = LookupPage(page_addr, Profiler::Overlap);
  assert(entry.index >= 0 && entry.index < ptt_capacity_);
  if (entry.state == PTTEntry::DIRTY_DIRECT ||
      entry.state == PTTEntry::DIRTY_STATIC) {
    --dirty_entries_;
  }
  entries_.erase(entry.mach_base);
  free_slots_.push_back(entry.index);
  assert(free_slots_.size() + entries_.size() == ptt_capacity_);
  pf.AddTableOp();
}

inline void MigrationController::Setup(
    Addr page_addr, PTTEntry::State state, Profiler& pf) {
  assert(PageAlign(page_addr) == page_addr);
  assert(!Contains(page_addr, Profiler::Overlap));

  PTTEntry& entry = entries_[page_addr];
  entry.index = free_slots_.back();
  free_slots_.pop_back();
  assert(free_slots_.size() + entries_.size() == ptt_capacity_);
  entry.state = state;
  if (state == PTTEntry::DIRTY_DIRECT || state == PTTEntry::DIRTY_STATIC) {
    ++dirty_entries_;
  }
  entry.mach_base = page_addr; // simulate direct/static page allocation
  assert(entries_.size() <= ptt_capacity_);
  pf.AddTableOp();
}

inline Addr MigrationController::Translate(Addr phy_addr,
    Addr page_base) const {
  assert((page_base & page_mask_) == 0);
  return page_base + (phy_addr & page_mask_);
}

#endif // SEXAIN_MIGRATION_CONTROLLER_H_
