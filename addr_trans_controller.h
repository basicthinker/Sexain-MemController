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

#ifdef MEMCK
struct AddrInfo {
  Addr phy_base;
  Addr mach_base;
  const char* state;
};
#endif

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

  virtual Addr LoadAddr(Addr phy_addr, Profiler& pf);
  virtual Control Probe(Addr phy_addr);
  virtual Addr StoreAddr(Addr phy_addr, int size, Profiler& pf);

  virtual void BeginCheckpointing(Profiler& pf);
  virtual void FinishCheckpointing();
  virtual void MigratePages(Profiler& pf, double dr = 0.33, double wr = 0.67);

  uint64_t Size() const;
  int block_size() const { return att_.block_size(); }
  int page_size() const { return migrator_.page_size(); }
  bool in_checkpointing() const { return in_checkpointing_; }
  const MigrationController& migrator() const { return migrator_; }

  uint64_t pages_to_dram() const { return pages_to_dram_; }
  uint64_t pages_to_nvm() const { return pages_to_nvm_; }

  virtual bool IsDRAM(Addr phy_addr, Profiler& pf);
#ifdef MEMCK
  std::pair<AddrInfo, AddrInfo> GetAddrInfo(Addr phy_addr);
#endif

 protected:
  AddrTransTable att_;
  VersionBuffer nvm_buffer_;
  VersionBuffer dram_buffer_;
  MigrationController migrator_;

 private:
  bool CheckValid(Addr phy_addr, int size);
  bool FullBlock(Addr phy_addr, int size);

  int Setup(Addr phy_addr, Addr mach_base, ATTEntry::State state,
      bool move_data, Profiler& pf);
  void HideClean(int index, bool move_data, Profiler& pf);
  Addr ResetClean(int index, bool move_data, Profiler& pf);
  void FreeClean(int index, Profiler& pf);
  Addr DirtyStained(int index, bool move_data, Profiler& pf);
  void FreeLoan(int index, bool move_data, Profiler& pf);
  void HideTemp(int index, bool move_data, Profiler& pf);

  Addr NVMStore(Addr phy_addr, int size, Profiler& pf);
  Addr DRAMStore(Addr phy_addr, int size, Profiler& pf);

  void Discard(int index, VersionBuffer& vb, Profiler& pf);
  /// Move a DRAM page out
  void MigrateDRAM(const DRAMPageStats& stats, Profiler& pf);
  /// Move a NVM page out
  void MigrateNVM(const NVMPageStats& stats, Profiler& pf);

  void CopyBlockIntra(Addr dest_addr, Addr src_addr, Profiler& pf);
  void CopyBlockInter(Addr dest_addr, Addr src_addr, Profiler& pf);
  void SwapBlock(Addr direct_addr, Addr mach_addr, Profiler& pf);

  const uint64_t phy_range_; ///< Size of physical address space
  const uint64_t dram_size_; ///< Size of DRAM cache region
  MemStore* mem_store_;
  bool in_checkpointing_;

  uint64_t pages_to_dram_; ///< Sum number of pages migrated from NVM to DRAM
  uint64_t pages_to_nvm_; ///< Sum number of pages migrated from DRAM to NVM

  class DirtyCleaner : public QueueVisitor { // inc. TEMP and HIDDEN
   public:
    DirtyCleaner(AddrTransController* atc, Profiler& pf) :
        atc_(atc), pf_(pf), num_flushed_entries_(0) { }
    void Visit(int i);
    int num_flushed_entries() const { return num_flushed_entries_; }
   private:
    AddrTransController* atc_;
    Profiler& pf_;
    int num_flushed_entries_;
  };

  class LoanRevoker : public QueueVisitor {
   public:
    LoanRevoker(AddrTransController* atc, Profiler& pf) : atc_(atc), pf_(pf) { }
    void Visit(int i);
   private:
    AddrTransController* atc_;
    Profiler& pf_;
  };
};

inline uint64_t AddrTransController::Size() const {
  return phy_range_ + nvm_buffer_.Size() + dram_buffer_.Size();
}

inline bool AddrTransController::IsDRAM(Addr phy_addr, Profiler& pf) {
  return migrator_.Contains(phy_addr, pf);
}

inline bool AddrTransController::CheckValid(Addr phy_addr, int size) {
  return att_.ToTag(phy_addr) == att_.ToTag(phy_addr + size - 1);
}

inline bool AddrTransController::FullBlock(Addr phy_addr, int size) {
  return (phy_addr & (att_.block_size() - 1)) == 0 && size == att_.block_size();
}

inline void AddrTransController::CopyBlockIntra(
    Addr dest_addr, Addr src_addr, Profiler& pf) {
  mem_store_->MemCopy(dest_addr, src_addr, att_.block_size());
  pf.AddBlockMoveIntra();
}

inline void AddrTransController::CopyBlockInter(
    Addr dest, Addr src, Profiler& pf) {
  mem_store_->MemCopy(dest, src, att_.block_size());
  pf.AddBlockMoveInter();
}

inline void AddrTransController::SwapBlock(
    Addr direct_addr, Addr mach_addr, Profiler& pf) {
  mem_store_->MemSwap(direct_addr, mach_addr, att_.block_size());
  pf.AddBlockMoveIntra(3);
}

#endif // SEXAIN_ADDR_TRANS_CONTROLLER_H_

