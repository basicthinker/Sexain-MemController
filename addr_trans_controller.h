// addr_trans_controller.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_ADDR_TRANS_CONTROLLER_H_
#define SEXAIN_ADDR_TRANS_CONTROLLER_H_

#include <cassert>
#include <vector>
#include "mem_store.h"
#include "addr_trans_table.h"

#define INVAL_ADDR uint64_t(-1)

enum AddrType {
  NVM_ADDR = 0,
  DRAM_ADDR = 1,
  RETRY_REQ = -1,
};

struct AddrStatus {
  enum AddrType type;
  enum ATTOperation oper;
};

class AddrTransController {
 public:
  AddrTransController(uint64_t dram_size, uint64_t nvm_size,
    AddrTransTable& blk_tbl, AddrTransTable& pg_tbl, MemStore* mem_store);
  AddrTransController(uint64_t phy_range, AddrTransTable& blk_tbl,
    AddrTransTable& pg_tbl, uint64_t dram_size, MemStore* mem_store);
  virtual ~AddrTransController() { }

  virtual uint64_t LoadAddr(uint64_t phy_addr);
  virtual uint64_t StoreAddr(uint64_t phy_addr, bool frozen = false);
  ///
  /// Predict the operation(s) for storing to the specified physical address
  ///
  virtual AddrStatus Probe(uint64_t phy_addr, bool frozen = false);
  virtual void NewEpoch();

  int cache_block_size() const { return block_table_.block_size(); }
  uint64_t phy_limit() const { return phy_limit_; }
  uint64_t dram_size() const { return dram_size_; }
  uint64_t nvm_size() const { return nvm_size_; }
  MemStore* mem_store() const { return mem_store_; }
  void set_mem_store(MemStore* mem_store) { mem_store_ = mem_store; }

 protected:
  virtual bool isDRAM(uint64_t phy_addr) { return phy_addr < dram_size_; }
  AddrTransTable& block_table_;
  AddrTransTable& page_table_; 

 private:
  uint64_t StoreDRAMAddr(uint64_t phy_addr);
  uint64_t StoreNVMAddr(uint64_t phy_addr, bool no_epoch = false);
  const uint64_t dram_size_;
  const uint64_t nvm_size_;
  const uint64_t phy_limit_;
  MemStore* mem_store_;
  ///
  /// Judge whether a DRAM physical addr is cross to NVM
  ///
  bool isCross(uint64_t dram_phy_addr);
  ///
  /// List of DRAM physical addresses
  /// that occupy the NVM block translation table
  ///
  std::vector<uint64_t> cross_list_;
};

// Space partition (low -> high):
// DRAM direct |(DRAM size)|| NVM direct |(phy_limit)||
// ATT buffer || DRAM backup || Page table buffer 
inline AddrTransController::AddrTransController(
    uint64_t dram_size, uint64_t nvm_size,
    AddrTransTable& blk_tbl, AddrTransTable& pg_tbl, MemStore* mem_store):

    block_table_(blk_tbl), page_table_(pg_tbl),
    dram_size_(dram_size), nvm_size_(nvm_size),
    phy_limit_(nvm_size - blk_tbl.image_size() - pg_tbl.image_size()),
    mem_store_(mem_store) {

  assert(phy_limit_ >= dram_size_);
  block_table_.set_image_floor(phy_limit());
  page_table_.set_image_floor(dram_size_ + nvm_size_ - pg_tbl.image_size());
  assert(block_table_.image_floor() + block_table_.image_size() ==
      page_table_.image_floor() - dram_size_);
}

inline AddrTransController::AddrTransController(uint64_t phy_range,
    AddrTransTable& blk_tbl, AddrTransTable& pg_tbl, uint64_t dram_size,
    MemStore* mem_store):

    AddrTransController(dram_size,
        phy_range + blk_tbl.image_size() + pg_tbl.image_size(),
        blk_tbl, pg_tbl, mem_store) {

  assert(phy_limit() == phy_range);
}

#endif // SEXAIN_ADDR_TRANS_CONTROLLER_H_

