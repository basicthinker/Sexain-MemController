// addr_trans_controller.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_ADDR_TRANS_CONTROLLER_H_
#define SEXAIN_ADDR_TRANS_CONTROLLER_H_

#include <cassert>
#include <vector>
#include "mem_store.h"
#include "version_buffer.h"
#include "addr_trans_table.h"
#include "migration_controller.h"
#include "profiler.h"

enum Control {
  REG_WRITE,
  NEW_EPOCH,
  WAIT_CKPT,
};

class AddrTransController {
 public:
  AddrTransController(uint64_t dram_size, Addr phy_limit,
      int att_len, int block_bits, int page_bits, MemStore* ms);
  virtual ~AddrTransController() { }

  virtual Addr LoadAddr(Addr phy_addr);
  virtual Control Probe(Addr phy_addr);
  virtual Addr StoreAddr(Addr phy_addr, int size);

  virtual void BeginCheckpointing(Profiler& profiler);
  virtual void FinishCheckpointing();
  virtual void MigratePages(Profiler& profiler,
      double dr = 0.33, double wr = 0.67);

  uint64_t Size() const;
  int block_size() const { return att_.block_size(); }
  int page_size() const { return migrator_.page_size(); }
  bool in_checkpointing() const { return in_checkpointing_; }
  const MigrationController& migrator() const { return migrator_; }

  virtual bool IsDRAM(Addr phy_addr, bool isTiming = true);

 protected:
  AddrTransTable att_;
  VersionBuffer nvm_buffer_;
  VersionBuffer dram_buffer_;
  MigrationController migrator_;

 private:
  bool CheckValid(Addr phy_addr, int size);
  bool FullBlock(Addr phy_addr, int size);

  int Setup(Addr phy_addr, Addr mach_base, ATTEntry::State state,
      bool move_data = true);
  void HideClean(int index, bool move_data = true);
  Addr ResetClean(int index, bool move_data = true);
  void FreeClean(int index);
  Addr DirtyStained(int index, bool move_data = true);
  void FreeLoan(int index, bool move_data = true);
  void HideTemp(int index, bool move_data = true);

  Addr NVMStore(Addr phy_addr, int size);
  Addr DRAMStore(Addr phy_addr, int size);

  void Discard(int index, VersionBuffer& vb, Profiler& profiler);
  /// Move a DRAM page out
  void MigrateDRAM(const DRAMPageStats& stats, Profiler& profiler);
  /// Move a NVM page out
  void MigrateNVM(const NVMPageStats& stats, Profiler& profiler);

  void CopyBlockIntra(Addr dest_addr, Addr src_addr, Profiler& profiler);
  void CopyBlockInter(Addr dest_addr, Addr src_addr, Profiler& profiler);
  void SwapBlock(Addr direct_addr, Addr mach_addr, Profiler& profiler);

  const uint64_t phy_range_; ///< Size of physical address space
  const uint64_t dram_size_; ///< Size of DRAM cache region
  MemStore* mem_store_;
  bool in_checkpointing_;

  class DirtyCleaner : public QueueVisitor { // inc. TEMP and HIDDEN
   public:
    DirtyCleaner(AddrTransController* atc) : atc_(atc), num_flushed_entries_(0) { }
    void Visit(int i);
    int num_flushed_entries() const { return num_flushed_entries_; }
   private:
    AddrTransController* atc_;
    int num_flushed_entries_;
  };

  class LoanRevoker : public QueueVisitor {
   public:
    LoanRevoker(AddrTransController* atc) : atc_(atc) { }
    void Visit(int i);
   private:
    AddrTransController* atc_;
  };
};

inline uint64_t AddrTransController::Size() const {
  return phy_range_ + nvm_buffer_.Size() + dram_buffer_.Size();
}

inline bool AddrTransController::IsDRAM(Addr phy_addr, bool isTiming) {
  if (isTiming) mem_store_->OnATTOp();
  return migrator_.Contains(phy_addr, Profiler::Null);
}

inline bool AddrTransController::CheckValid(Addr phy_addr, int size) {
  return att_.ToTag(phy_addr) == att_.ToTag(phy_addr + size - 1);
}

inline bool AddrTransController::FullBlock(Addr phy_addr, int size) {
  return (phy_addr & (att_.block_size() - 1)) == 0 && size == att_.block_size();
}

inline void AddrTransController::CopyBlockIntra(
    Addr dest_addr, Addr src_addr, Profiler& profiler) {
  mem_store_->MemCopy(dest_addr, src_addr, att_.block_size());
  profiler.AddBlockMoveIntra();
}

inline void AddrTransController::CopyBlockInter(
    Addr dest, Addr src, Profiler& profiler) {
  mem_store_->MemCopy(dest, src, att_.block_size());
  profiler.AddBlockMoveInter();
}

inline void AddrTransController::SwapBlock(
    Addr direct_addr, Addr mach_addr, Profiler& profiler) {
  mem_store_->MemSwap(direct_addr, mach_addr, att_.block_size());

}

#endif // SEXAIN_ADDR_TRANS_CONTROLLER_H_

