// access_profiler.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_ACCESS_PROFILER_H_
#define SEXAIN_ACCESS_PROFILER_H_

#include <cassert>
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>

class AccessProfiler {
 public:
  struct BlockStats {
    int num_reads;
    int num_writes;
    BlockStats() : num_reads(0), num_writes(0) { }
  };

  struct PageStats {
    int num_reads;
    int num_writes;
    std::set<uint64_t> blocks;
    PageStats() : num_reads(0), num_writes(0) { }
  };

  struct Stats {
    uint64_t addr;
    double dirty_ratio;
    double write_ratio;
  };

  AccessProfiler(int block_bits, int page_bits);
  void Read(uint64_t addr, bool is_static = false);
  void Write(uint64_t addr, bool is_static = false);

  int num_blocks(bool is_static = false) const {
    return block_stats_[is_static].size();
  }

  int num_pages(bool is_static = false) const {
    return page_stats_[is_static].size();
  }

  int total_reads() const { return total_reads_; }
  int total_writes() const { return total_writes_; }

  /// Next NVM pages with decreasing dirty ratio
  Stats ExtractSource();
  /// Next DRAM page with increasing dirty ratio
  Stats ExtractDestination();
  void Reset();

 private:
  void MoveData(std::unordered_map<uint64_t, BlockStats>& blocks,
      std::unordered_map<uint64_t, PageStats>& pages);
  uint64_t BlockAlign(uint64_t addr);
  uint64_t PageAlign(uint64_t addr);

  struct {
    bool operator()(const Stats& a, const Stats& b) {
      return a.dirty_ratio < b.dirty_ratio;
    }
  } comp_;

  struct {
    bool operator()(const Stats& a, const Stats& b) {
      return a.dirty_ratio > b.dirty_ratio;
    }
  } reverse_comp_;

  template <class Compare> 
  void GenerateHeap(std::unordered_map<uint64_t, PageStats>& pages,
      std::vector<Stats>& heap, Compare comp);

  const int block_bits_;
  const uint64_t block_mask_;
  const int page_bits_;
  const uint64_t page_mask_;

  int total_reads_;
  int total_writes_;

  std::vector<std::unordered_map<uint64_t, BlockStats>> block_stats_;
  std::vector<std::unordered_map<uint64_t, PageStats>> page_stats_;
  std::vector<Stats> sources_; ///< NVM pages
  std::vector<Stats> destinations_; ///< DRAM pages
};

inline AccessProfiler::AccessProfiler(int block_bits, int page_bits) :
    block_bits_(block_bits), block_mask_((1 << block_bits) - 1),
    page_bits_(page_bits), page_mask_((1 << page_bits) - 1),
    block_stats_(2), page_stats_(2) {
  total_reads_ = 0;
  total_writes_ = 0;
}

inline void AccessProfiler::Read(uint64_t addr, bool is_static) {
  ++block_stats_[is_static][BlockAlign(addr)].num_reads;
}

inline void AccessProfiler::Write(uint64_t addr, bool is_static) {
  ++block_stats_[is_static][BlockAlign(addr)].num_writes;
}

inline AccessProfiler::Stats AccessProfiler::ExtractSource() {
  if (page_stats_[0].empty()) {
    MoveData(block_stats_[0], page_stats_[0]);
    GenerateHeap(page_stats_[0], sources_, comp_);
  }

  if (sources_.empty()) return {(uint64_t)-EINVAL, -EINVAL, -EINVAL};

  std::pop_heap(sources_.begin(), sources_.end(), comp_);
  Stats s = sources_.back();
  sources_.pop_back();
  return s;
}

inline AccessProfiler::Stats AccessProfiler::ExtractDestination() {
  if (page_stats_[1].empty()) {
    MoveData(block_stats_[1], page_stats_[1]);
    GenerateHeap(page_stats_[1], destinations_, reverse_comp_);
  }

  if (destinations_.empty()) return {(uint64_t)-EINVAL, -EINVAL, -EINVAL};

  std::pop_heap(destinations_.begin(), destinations_.end(), reverse_comp_);
  Stats d = destinations_.back();
  destinations_.pop_back();
  return d;
}

inline void AccessProfiler::MoveData(
    std::unordered_map<uint64_t, BlockStats>& blocks,
    std::unordered_map<uint64_t, PageStats>& pages) {
  assert(pages.empty());
  for (std::unordered_map<uint64_t, BlockStats>::iterator it = blocks.begin();
      it != blocks.end(); ++it) {
    PageStats& ps = pages[PageAlign(it->first)];
    ps.num_reads += it->second.num_reads;
    total_reads_ += it->second.num_reads;
    ps.num_writes += it->second.num_writes;
    total_writes_ += it->second.num_writes;
    if (it->second.num_writes) ps.blocks.insert(it->first);
  }
}

template <class Compare> 
inline void AccessProfiler::GenerateHeap(
    std::unordered_map<uint64_t, PageStats>& pages,
    std::vector<AccessProfiler::Stats>& heap, Compare comp) {
  assert(heap.empty());
  double total_blocks = 1 << (page_bits_ - block_bits_);
  for (std::unordered_map<uint64_t, PageStats>::iterator it = pages.begin();
      it != pages.end(); ++it) {
    double dr = it->second.blocks.size() / total_blocks;
    double wr = (double)it->second.num_writes / it->second.blocks.size();
    heap.push_back({it->first, dr, wr});
  }
  std::make_heap(heap.begin(), heap.end(), comp);
}

inline void AccessProfiler::Reset() {
  block_stats_[0].clear();
  block_stats_[1].clear();
  page_stats_[0].clear();
  page_stats_[1].clear();
  sources_.clear();
  destinations_.clear();
}

inline uint64_t AccessProfiler::BlockAlign(uint64_t addr) {
  return addr & ~block_mask_;
}

inline uint64_t AccessProfiler::PageAlign(uint64_t addr) {
  return addr & ~page_mask_;
}

#endif // SEXAIN_ACCESS_PROFILER_H_

