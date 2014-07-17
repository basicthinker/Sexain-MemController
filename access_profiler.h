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

  AccessProfiler(int block_bits, int page_bits);
  void Read(uint64_t addr, bool is_static = false);
  void Write(uint64_t addr, bool is_static = false);

  int num_blocks(bool is_static = false) const {
    return block_stats_[is_static].size();
  }

  int num_pages(bool is_static = false) const {
    return page_stats_[is_static].size();
  }

  int epoch_reads() const { return epoch_reads_; }
  int epoch_writes() const { return epoch_writes_; }

  /// Collect NVM pages above the specified dirty ratio
  std::vector<std::pair<uint64_t, double>> ExtractSources(size_t num);
  /// Collect DRAM pages below the specified dirty ratio
  std::vector<std::pair<uint64_t, double>> ExtractDestinations(size_t num);
  void Reset();

 private:
  void MoveData(std::unordered_map<uint64_t, BlockStats>& blocks,
      std::unordered_map<uint64_t, PageStats>& pages);
  uint64_t BlockAlign(uint64_t addr);
  uint64_t PageAlign(uint64_t addr);

  struct {
    bool operator()(const std::pair<uint64_t, double>& a,
        const std::pair<uint64_t, double>& b) {
      return a.second < b.second;
    }
  } comp_;

  struct {
    bool operator()(const std::pair<uint64_t, double>& a,
        const std::pair<uint64_t, double>& b) {
      return a.second > b.second;
    }
  } reverse_comp_;

  template <class Compare> 
  void GenerateHeap(std::unordered_map<uint64_t, PageStats>& pages,
      std::vector<std::pair<uint64_t, double>>& heap, Compare comp);

  const int block_bits_;
  const uint64_t block_mask_;
  const int page_bits_;
  const uint64_t page_mask_;

  int epoch_reads_;
  int epoch_writes_;

  std::vector<std::unordered_map<uint64_t, BlockStats>> block_stats_;
  std::vector<std::unordered_map<uint64_t, PageStats>> page_stats_;
  std::vector<std::pair<uint64_t, double>> sources_; ///< NVM pages
  std::vector<std::pair<uint64_t, double>> destinations_; ///< DRAM pages
};

inline AccessProfiler::AccessProfiler(int block_bits, int page_bits) :
    block_bits_(block_bits), block_mask_((1 << block_bits) - 1),
    page_bits_(page_bits), page_mask_((1 << page_bits) - 1),
    block_stats_(2), page_stats_(2) {
  epoch_reads_ = 0;
  epoch_writes_ = 0;
  sources_.reserve(1 << page_bits_);
  destinations_.reserve(1 << page_bits_);
}

inline void AccessProfiler::Read(uint64_t addr, bool is_static) {
  ++block_stats_[is_static][BlockAlign(addr)].num_reads;
}

inline void AccessProfiler::Write(uint64_t addr, bool is_static) {
  ++block_stats_[is_static][BlockAlign(addr)].num_writes;
}

inline std::vector<std::pair<uint64_t, double>>
AccessProfiler::ExtractSources(size_t num) {
  if (page_stats_[0].empty()) {
    MoveData(block_stats_[0], page_stats_[0]);
    GenerateHeap(page_stats_[0], sources_, comp_);
  }

  if(num > sources_.size()) num = sources_.size();
  std::vector<std::pair<uint64_t, double>> sorted;
  for (unsigned int i = 0; i < num; ++i) {
    std::pop_heap(sources_.begin(), sources_.end(), comp_);
    sorted.push_back(sources_.back());
    sources_.pop_back();
  }
  return sorted;
}

inline std::vector<std::pair<uint64_t, double>>
AccessProfiler::ExtractDestinations(size_t num) {
  if (page_stats_[1].empty()) {
    MoveData(block_stats_[1], page_stats_[1]);
    GenerateHeap(page_stats_[1], destinations_, reverse_comp_);
  }

  if (num > destinations_.size()) num = destinations_.size();
  std::vector<std::pair<uint64_t, double>> sorted;
  for (unsigned int i = 0; i < num; ++i) {
    std::pop_heap(destinations_.begin(), destinations_.end(), reverse_comp_);
    sorted.push_back(destinations_.back());
    destinations_.pop_back();
  }
  return sorted;
}

inline void AccessProfiler::MoveData(
    std::unordered_map<uint64_t, BlockStats>& blocks,
    std::unordered_map<uint64_t, PageStats>& pages) {
  assert(pages.empty());
  for (std::unordered_map<uint64_t, BlockStats>::iterator it = blocks.begin();
      it != blocks.end(); ++it) {
    PageStats& ps = pages[PageAlign(it->first)];
    ps.num_reads += it->second.num_reads;
    epoch_reads_ += it->second.num_reads;
    ps.num_writes += it->second.num_writes;
    epoch_writes_ += it->second.num_writes;
    if (it->second.num_writes) ps.blocks.insert(it->first);
  }
}

template <class Compare> 
inline void AccessProfiler::GenerateHeap(
    std::unordered_map<uint64_t, PageStats>& pages,
    std::vector<std::pair<uint64_t, double>>& heap, Compare comp) {
  assert(heap.empty());
  double total_blocks = 1 << (page_bits_ - block_bits_);
  for (std::unordered_map<uint64_t, PageStats>::iterator it = pages.begin();
      it != pages.end(); ++it) {
    double r = it->second.blocks.size() / total_blocks;
    heap.push_back(std::make_pair(it->first, r));
  }
  std::make_heap(heap.begin(), heap.end(), comp);
}

inline void AccessProfiler::Reset() {
  epoch_reads_ = 0;
  epoch_writes_ = 0;
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

