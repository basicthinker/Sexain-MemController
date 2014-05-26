// addr_trans_controller.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_ADDR_TRANS_CONTROLLER_H_
#define SEXAIN_ADDR_TRANS_CONTROLLER_H_

#include <cassert>
#include <vector>
#include "version_buffer.h"
#include "addr_trans_table.h"
#include "mem_store.h"

enum ATTControl {
  ATC_ACCEPT,
  ATC_EPOCH,
  ATC_RETRY,
};

class AddrTransController {
 public:
  AddrTransController(uint64_t dram_size, Addr phy_limit,
      int att_len, int block_bits, int ptt_len, int page_bits, MemStore* ms);
  virtual ~AddrTransController() { }

  virtual Addr LoadAddr(Addr phy_addr);
  virtual Addr StoreAddr(Addr phy_addr, int size);
  virtual ATTControl Probe(Addr phy_addr);

  virtual void BeginEpochEnding();
  virtual void FinishEpochEnding();

  uint64_t Size() const;
  Addr PhyLimit() const { return dram_size_ + nvm_size_; }
  int cache_block_size() const { return att_.block_size(); }
  bool in_ending() const { return in_ending_; }

 protected:
  virtual bool isDRAM(Addr phy_addr);
  AddrTransTable att_;
  VersionBuffer nvm_buffer_;
  VersionBuffer dram_buffer_;
  AddrTransTable ptt_;
  VersionBuffer ptt_buffer_;

 private:
  bool CheckValid(Addr phy_addr, int size);
  void ATTSetup(Tag phy_tag, Addr mach_base, int size,
      ATTEntry::State state, ATTEntry::SubState sub); 
  void ATTShrink(int index, bool move_data = true);
  Addr ATTRevoke(int index, bool move_data = true);

  Addr NVMStore(Addr phy_addr, int size);
  Addr DRAMStore(Addr phy_addr, int size);
  void PseudoPageStore(Addr phy_addr);

  const uint64_t dram_size_; ///< Size of visible DRAM region
  const uint64_t nvm_size_; ///< Size of visible NVM region
  MemStore* mem_store_;
  bool in_ending_;

  int num_page_move_; ///< Move of NVM pages
  
  class TempEntryRevoker : public QueueVisitor {
   public:
    TempEntryRevoker(AddrTransController& atc) : atc_(atc) { }
    void Visit(int i) { atc_.ATTRevoke(i, true); }
   private:
    AddrTransController& atc_;
  };

  class DirtyEntryRevoker : public QueueVisitor {
   public:
    DirtyEntryRevoker(AddrTransController& atc) : atc_(atc) { }
    void Visit(int i);
   private:
    AddrTransController& atc_;
  };

  class DirtyEntryCleaner : public QueueVisitor {
   public:
    DirtyEntryCleaner(AddrTransTable& att) : att_(att) { }
    void Visit(int i);
   private:
    AddrTransTable& att_;
  };
};

// Space partition (low -> high):
// Visible DRAM || Visible NVM (phy_limit)||
// DRAM backup || PTT buffer || Temporary buffer || ATT buffer
inline AddrTransController::AddrTransController(
    uint64_t dram_size, Addr phy_limit,
    int att_len, int block_bits, int ptt_len, int page_bits, MemStore* ms):
    att_(att_len, block_bits), nvm_buffer_(2 * att_len, block_bits),
    dram_buffer_(att_len, block_bits),
    ptt_(ptt_len, page_bits), ptt_buffer_(2 * ptt_len, page_bits),
    dram_size_(dram_size), nvm_size_(phy_limit - dram_size) {

  assert(phy_limit >= dram_size);
  mem_store_ = ms;
  in_ending_ = false;
  num_page_move_ = 0;

  ptt_buffer_.set_addr_base(PhyLimit() + dram_size_);
  dram_buffer_.set_addr_base(ptt_buffer_.addr_base() + ptt_buffer_.Size());
  nvm_buffer_.set_addr_base(dram_buffer_.addr_base() + dram_buffer_.Size());
}

inline uint64_t AddrTransController::Size() const {
  return nvm_buffer_.addr_base() + nvm_buffer_.Size();
}

inline bool AddrTransController::isDRAM(Addr phy_addr) {
  return phy_addr < dram_size_;
}

inline bool AddrTransController::CheckValid(Addr phy_addr, int size) {
  return att_.ToTag(phy_addr) == att_.ToTag(phy_addr + size - 1);
}

inline void AddrTransController::DirtyEntryRevoker::Visit(int i) {
  if (atc_.att_.At(i).IsPlaceholder()) {
    atc_.ATTRevoke(i, true);
  }
}

inline void AddrTransController::DirtyEntryCleaner::Visit(int i) {
  if (att_.At(i).IsRegularDirty()) {
    att_.CleanEntry(i);
  }
}

#endif // SEXAIN_ADDR_TRANS_CONTROLLER_H_

