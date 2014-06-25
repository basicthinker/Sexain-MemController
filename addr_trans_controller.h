// addr_trans_controller.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_ADDR_TRANS_CONTROLLER_H_
#define SEXAIN_ADDR_TRANS_CONTROLLER_H_

#include <cassert>
#include <vector>
#include "version_buffer.h"
#include "addr_trans_table.h"
#include "mem_store.h"

class AddrTransController {
 public:
  enum Control {
    ACCEPT,
    EPOCH,
    RETRY,
  };

  AddrTransController(uint64_t dram_size, Addr phy_limit,
      int att_len, int block_bits, int ptt_len, int page_bits, MemStore* ms);
  virtual ~AddrTransController() { }

  virtual Addr LoadAddr(Addr phy_addr);
  virtual Addr StoreAddr(Addr phy_addr, int size);
  virtual Control Probe(Addr phy_addr);

  virtual void BeginCheckpointing();
  virtual void FinishCheckpointing();

  uint64_t Size() const;
  Addr PhyLimit() const { return dram_size_ + nvm_size_; }
  int cache_block_size() const { return att_.block_size(); }
  bool in_checkpointing() const { return in_checkpointing_; }

 protected:
  virtual bool IsDRAM(Addr phy_addr);
  AddrTransTable att_;
  VersionBuffer nvm_buffer_;
  VersionBuffer dram_buffer_;
  AddrTransTable ptt_;

 private:
  bool CheckValid(Addr phy_addr, int size);
  bool FullBlock(Addr phy_addr, int size);

  void Setup(Addr phy_addr, Addr mach_base, int size, ATTEntry::State state);
  void HideClean(int index, bool move_data = true);
  Addr ResetClean(int index, bool move_data = true);
  void FreeClean(int index, bool move_data = true);
  void FreeLoan(int index, bool move_data = true);
  void HideTemp(int index, bool move_data = true);

  Addr NVMStore(Addr phy_addr, int size);
  Addr DRAMStore(Addr phy_addr, int size);
  void PseudoPageStore(Tag phy_tag);

  const uint64_t dram_size_; ///< Size of direct DRAM region
  const uint64_t nvm_size_; ///< Size of direct NVM region
  MemStore* mem_store_;
  bool in_checkpointing_;

  int pages_twice_written_; ///< Number of twice-write pages

  class DirtyCleaner : public QueueVisitor { // inc. TEMP and HIDDEN
   public:
    DirtyCleaner(AddrTransController& atc) : atc_(atc) { }
    void Visit(int i);
   private:
    AddrTransController& atc_;
  };

  class LoanRevoker : public QueueVisitor {
   public:
    LoanRevoker(AddrTransController& atc) : atc_(atc) { }
    void Visit(int i);
   private:
    AddrTransController& atc_;
  };

  class PTTCleaner : public QueueVisitor {
   public:
    PTTCleaner(AddrTransTable& ptt) : ptt_(ptt) { }
    void Visit(int i);
   private:
    AddrTransTable& ptt_;
  };
};

// Space partition (low -> high):
// Direct DRAM || Direct NVM (phy_limit)||
// DRAM backup || DRAM buffer || NVM buffer
inline AddrTransController::AddrTransController(
    uint64_t dram_size, Addr phy_limit,
    int att_len, int block_bits, int ptt_len, int page_bits, MemStore* ms):
        att_(att_len, block_bits), nvm_buffer_(2 * att_len, block_bits),
        dram_buffer_(att_len, block_bits), ptt_(ptt_len, page_bits),
        dram_size_(dram_size), nvm_size_(phy_limit - dram_size) {

  assert(phy_limit >= dram_size);
  mem_store_ = ms;
  in_checkpointing_ = false;
  pages_twice_written_ = 0;

  dram_buffer_.set_addr_base(PhyLimit() + dram_size_);
  nvm_buffer_.set_addr_base(dram_buffer_.addr_base() + dram_buffer_.Size());
}

inline uint64_t AddrTransController::Size() const {
  return nvm_buffer_.addr_base() + nvm_buffer_.Size();
}

inline bool AddrTransController::IsDRAM(Addr phy_addr) {
  return phy_addr < dram_size_;
}

inline bool AddrTransController::CheckValid(Addr phy_addr, int size) {
  return att_.ToTag(phy_addr) == att_.ToTag(phy_addr + size - 1);
}

inline bool AddrTransController::FullBlock(Addr phy_addr, int size) {
  return (phy_addr & (att_.block_size() - 1)) == 0 && size == att_.block_size();
}

inline void AddrTransController::DirtyCleaner::Visit(int i) {
  const ATTEntry& entry = atc_.att_.At(i);
  if (entry.state == ATTEntry::DIRTY) {
    atc_.att_.ShiftState(i, ATTEntry::CLEAN);
  } else if (entry.state == ATTEntry::TEMP) {
    atc_.HideTemp(i);
  }

  if (entry.state == ATTEntry::HIDDEN) {
    atc_.att_.ShiftState(i, ATTEntry::FREE);
  }
}

inline void AddrTransController::LoanRevoker::Visit(int i) {
  const ATTEntry& entry = atc_.att_.At(i);
  assert(entry.state == ATTEntry::LOAN);
  atc_.FreeLoan(i);
}

inline void AddrTransController::PTTCleaner::Visit(int i) {
  const ATTEntry& entry = ptt_.At(i);
  if (entry.state == ATTEntry::DIRTY) {
    ptt_.ShiftState(i, ATTEntry::CLEAN);
  } else {
    assert(entry.state == ATTEntry::HIDDEN);
    ptt_.ShiftState(i, ATTEntry::FREE);
  }
}

#endif // SEXAIN_ADDR_TRANS_CONTROLLER_H_

