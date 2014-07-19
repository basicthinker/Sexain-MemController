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

  PTTEntry() : epoch_reads(0), epoch_writes(0) { }
};

typedef std::unordered_map<uint64_t, PTTEntry>::iterator PTTEntryPtr;

struct PageStats {
  uint64_t phy_addr;
  PTTEntry::State state;
  double dirty_ratio;
  double write_ratio;
};

class MigrationController {
 public:
  MigrationController(int block_bits, int page_bits, int ptt_limit);

  PTTEntryPtr LookupPage(uint64_t phy_addr);
  bool IsValid(PTTEntryPtr entry);
  bool Contains(uint64_t phy_addr);
  void OnDRAMRead(PTTEntryPtr entry);
  void OnDRAMWrite(PTTEntryPtr entry);
  void ShiftState(PTTEntryPtr entry, PTTEntry::State state);
  void Free(uint64_t page_addr);
  void Setup(uint64_t page_addr, PTTEntry::State state);

  /// Calculate statistics over the blocks from ATT
  void InputBlocks(const std::vector<ATTEntry>& blocks);
  /// Next NVM pages with decreasing dirty ratio
  bool ExtractNVMPage(PageStats& stats);
  /// Next DRAM page with increasing dirty ratio
  bool ExtractDRAMPage(PageStats& stats);
  /// Clean up all entries, heaps, epoch statistics, etc.
  /// \return the number of dirty pages to write back
  int Clean();

  int page_size() const { return 1 << page_bits_; }
  int total_writes() const { return total_writes_; }

 private:
  struct NVMPage {
    int epoch_reads;
    int epoch_writes;
    std::set<uint64_t> blocks;
  };

  uint64_t BlockAlign(uint64_t addr) { return addr & ~block_mask_; }
  uint64_t PageAlign(uint64_t addr) { return addr & ~page_mask_; }

  struct {
    bool operator()(const PageStats& a, const PageStats& b) {
      if (a.dirty_ratio == b.dirty_ratio) {
        return a.state < b.state;
      } else return a.dirty_ratio < b.dirty_ratio;
    }
  } dram_comp_;

  struct {
    bool operator()(const PageStats& a, const PageStats& b) {
      return a.dirty_ratio > b.dirty_ratio;
    }
  } nvm_comp_;

  const int block_bits_;
  const uint64_t block_mask_;
  const int page_bits_;
  const uint64_t page_mask_;
  const int ptt_limit_;

  int dirty_pages_; ///< Number of dirty pages each epoch
  int total_writes_; ///< Sum number of DRAM writes received

  std::unordered_map<uint64_t, PTTEntry> entries_;
  std::unordered_map<uint64_t, NVMPage> nvm_pages_;
  std::vector<PageStats> dram_heap_;
  std::vector<PageStats> nvm_heap_;
};

inline MigrationController::MigrationController(
    int block_bits, int page_bits, int ptt_limit) :

    block_bits_(block_bits), block_mask_((1 << block_bits) - 1),
    page_bits_(page_bits), page_mask_((1 << page_bits) - 1),
    ptt_limit_(ptt_limit), dirty_pages_(0), total_writes_(0) {
}

inline PTTEntryPtr MigrationController::LookupPage(uint64_t phy_addr) {
  return entries_.find(PageAlign(phy_addr));
}

inline bool MigrationController::IsValid(PTTEntryPtr entry) {
  return entry != entries_.end();
}

inline bool MigrationController::Contains(uint64_t phy_addr) {
  return entries_.find(PageAlign(phy_addr)) != entries_.end();
}

inline void MigrationController::OnDRAMRead(PTTEntryPtr entry) {
  ++entry->second.epoch_reads;
}

inline void MigrationController::OnDRAMWrite(PTTEntryPtr entry) {
  ++entry->second.epoch_writes;
}

inline void MigrationController::ShiftState(
    PTTEntryPtr entry, PTTEntry::State state) {
  entry->second.state = state;
  if (state == PTTEntry::DIRTY_DIRECT || state == PTTEntry::DIRTY_STATIC) {
    ++dirty_pages_;
  }
}

inline void MigrationController::Free(uint64_t page_addr) {
  PTTEntryPtr p = LookupPage(page_addr);
  assert(IsValid(p) && p->first == page_addr);
  if (p->second.state == PTTEntry::DIRTY_DIRECT ||
      p->second.state == PTTEntry::DIRTY_STATIC) {
    --dirty_pages_;
  }
  entries_.erase(p);
}

inline void MigrationController::Setup(
    uint64_t page_addr, PTTEntry::State state) {
  assert((page_addr & page_mask_) == 0);
  entries_[page_addr].state = state;
  if (state == PTTEntry::DIRTY_DIRECT || state == PTTEntry::DIRTY_STATIC) {
    ++dirty_pages_;
  }
  assert(entries_.size() <= ptt_limit_);
}

#endif // SEXAIN_MIGRATION_CONTROLLER_H_
