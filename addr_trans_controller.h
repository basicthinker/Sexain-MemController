// addr_trans_controller.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_ADDR_TRANS_CONTROLLER_H_
#define SEXAIN_ADDR_TRANS_CONTROLLER_H_

#include <cassert>
#include <vector>
#include "version_buffer.h"
#include "addr_trans_table.h"
#include "mem_store.h"

enum ATTState {
  RETRY,
  EPOCH,
  AVAIL,
};

class AddrTransController {
 public:
  AddrTransController(uint64_t dram_size, uint64_t phy_limit,
      int att_len, int block_bits, int ptt_len, int page_bits, MemStore* ms);
  virtual ~AddrTransController() { }

  virtual uint64_t LoadAddr(uint64_t phy_addr);
  virtual uint64_t StoreAddr(uint64_t phy_addr, bool frozen);
  virtual ATTState Probe(uint64_t phy_addr, bool frozen);
  virtual void NewEpoch();

  uint64_t Size() const;
  int cache_block_size() const { return att_.block_size(); }
  uint64_t phy_limit() const { return dram_size_ + nvm_size_; }

 protected:
  virtual bool isDRAM(uint64_t phy_addr);
  AddrTransTable att_;
  VersionBuffer nvm_buffer_;
  VersionBuffer dram_buffer_;
  AddrTransTable ptt_;
  VersionBuffer ptt_buffer_;

 private:
  static uint64_t NVMStore(uint64_t phy_addr,
      AddrTransTable* att, VersionBuffer* vb, MemStore* ms);
  static bool ProbeNVMStore(uint64_t phy_addr,
      AddrTransTable* att, VersionBuffer* vb, MemStore* ms);
  static uint64_t DRAMStore(uint64_t phy_addr, bool frozen,
      AddrTransTable* att, VersionBuffer* vb, MemStore* ms);
  static bool ProbeDRAMStore(uint64_t phy_addr, bool frozen,
      AddrTransTable* att, VersionBuffer* vb, MemStore* ms);
  const uint64_t dram_size_; ///< Size of visible DRAM region
  const uint64_t nvm_size_; ///< Size of visible NVM region
  MemStore* mem_store_;
  ///
  /// List of DRAM physical addresses that occupy ATT
  ///
  std::vector<uint64_t> cross_list_;
};

// Space partition (low -> high):
// Visible DRAM || Visible NVM (phy_limit)||
// DRAM backup || PTT buffer || Temporary buffer || ATT buffer
inline AddrTransController::AddrTransController(
    uint64_t dram_size, uint64_t phy_size,
    int att_len, int block_bits, int ptt_len, int page_bits, MemStore* ms):
    att_(att_len, block_bits), nvm_buffer_(2 * att_len, block_bits),
    dram_buffer_(att_len, block_bits),
    ptt_(ptt_len, page_bits), ptt_buffer_(2 * ptt_len, page_bits),
    dram_size_(dram_size), nvm_size_(phy_size - dram_size) {
  assert(phy_size >= dram_size);
  mem_store_ = ms;
  ptt_buffer_.set_addr_base(phy_limit() + dram_size_);
  dram_buffer_.set_addr_base(ptt_buffer_.addr_base() + ptt_buffer_.Size());
  nvm_buffer_.set_addr_base(dram_buffer_.addr_base() + dram_buffer_.Size());
}

inline uint64_t AddrTransController::Size() const {
  return nvm_buffer_.addr_base() + nvm_buffer_.Size();
}

inline bool AddrTransController::isDRAM(uint64_t phy_addr) {
  return phy_addr < dram_size_;
}

#endif // SEXAIN_ADDR_TRANS_CONTROLLER_H_

