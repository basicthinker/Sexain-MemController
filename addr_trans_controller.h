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

  virtual void BeginCheckpointing();
  virtual void FinishCheckpointing();
  virtual void MigratePages(double threshold, Profiler& profiler);

  uint64_t Size() const;
  int block_size() const { return att_.block_size(); }
  int page_size() const { return migrator_.page_size(); }
  bool in_checkpointing() const { return in_checkpointing_; }

  virtual bool IsDRAM(Addr phy_addr, bool isTiming = true);

 protected:
  AddrTransTable att_;
  VersionBuffer nvm_buffer_;
  VersionBuffer dram_buffer_;
  MigrationController migrator_;

 private:
  bool CheckValid(Addr phy_addr, int size);
  bool FullBlock(Addr phy_addr, int size);

  int Setup(Addr phy_addr, Addr mach_base, int size, ATTEntry::State state);
  void HideClean(int index, bool move_data = true);
  Addr ResetClean(int index, bool move_data = true);
  void FreeClean(int index, bool move_data = true);
  Addr DirtyStained(int index, bool move_data = true);
  void FreeLoan(int index, bool move_data = true);
  void HideTemp(int index, bool move_data = true);

  Addr NVMStore(Addr phy_addr, int size);
  Addr DRAMStore(Addr phy_addr, int size);
  bool PseudoPageStore(Addr phy_addr);

  /// Move a DRAM page out
  void MigrateDRAM(const DRAMPageStats& stats, Profiler& profiler);
  /// Move a NVM page out
  void MigrateNVM(const NVMPageStats& stats, Profiler& profiler);

  ///
  /// Wrappers with timings
  ///
  void MoveToDRAM(uint64_t destination, uint64_t source, int size);
  void MoveToNVM(uint64_t destination, uint64_t source, int size);
  void SwapNVM(uint64_t static_addr, uint64_t mach_addr, int size);
  std::pair<int, Addr> ATTLookup(AddrTransTable& att, Tag phy_tag);
  int ATTSetup(AddrTransTable& att,
      Tag phy_tag, Addr mach_base, ATTEntry::State state);
  void ATTShiftState(AddrTransTable& att, int index, ATTEntry::State state);
  void ATTReset(AddrTransTable& att,
      int index, Addr new_base, ATTEntry::State new_state);
  void ATTVisit(AddrTransTable& att,
      ATTEntry::State state, QueueVisitor* visitor);
  int ATTFront(AddrTransTable& att, ATTEntry::State state);
  /// VersionBuffer
  uint64_t VBNewBlock(VersionBuffer& vb);
  void VBFreeBlock(VersionBuffer& vb,
      uint64_t mach_addr, VersionBuffer::State state);
  void VBBackupBlock(VersionBuffer& vb,
      uint64_t mach_addr, VersionBuffer::State state);
  void VBClearBackup(VersionBuffer& vb);

  const uint64_t phy_range_; ///< Size of physical address space
  const uint64_t dram_size_; ///< Size of DRAM cache region
  MemStore* mem_store_;
  bool in_checkpointing_;

  class DirtyCleaner : public QueueVisitor { // inc. TEMP and HIDDEN
   public:
    DirtyCleaner(AddrTransController* atc) : atc_(atc), num_new_entries_(0) { }
    void Visit(int i);
    int num_new_entries() const { return num_new_entries_; }
   private:
    AddrTransController* atc_;
    int num_new_entries_;
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

// Wrapper functions for AddrTransTable
inline void AddrTransController::MoveToDRAM(
    uint64_t destination, uint64_t source, int size) {
  mem_store_->DoMove(destination, source, size);
  mem_store_->OnDRAMWrite(destination, size);
}

inline void AddrTransController::MoveToNVM(
    uint64_t destination, uint64_t source, int size) {
  mem_store_->DoMove(destination, source, size);
  mem_store_->OnNVMWrite(destination, size);
}

inline void AddrTransController::SwapNVM(
    uint64_t static_addr, uint64_t mach_addr, int size) {
  mem_store_->DoSwap(static_addr, mach_addr, size);
  mem_store_->OnNVMWrite(static_addr, size);
  mem_store_->OnNVMWrite(mach_addr, size);
}

inline std::pair<int, Addr> AddrTransController::ATTLookup(AddrTransTable& att,
    Tag phy_tag) {
  mem_store_->OnATTOp();
  return att.Lookup(phy_tag);
}

inline int AddrTransController::ATTSetup(AddrTransTable& att,
    Tag phy_tag, Addr mach_base, ATTEntry::State state) {
  mem_store_->OnATTOp();
  return att.Setup(phy_tag, mach_base, state);
}

inline void AddrTransController::ATTShiftState(AddrTransTable& att,
    int index, ATTEntry::State state) {
  mem_store_->OnATTOp();
  att.ShiftState(index, state);
}

inline void AddrTransController::ATTReset(AddrTransTable& att,
    int index, Addr new_base, ATTEntry::State new_state) {
  mem_store_->OnATTOp();
  att.Reset(index, new_base, new_state);
}

inline int AddrTransController::ATTFront(AddrTransTable& att,
    ATTEntry::State state) {
  mem_store_->OnATTOp();
  return att.GetFront(state);
}

inline void AddrTransController::ATTVisit(AddrTransTable& att,
    ATTEntry::State state, QueueVisitor* visitor) {
  int num = att.VisitQueue(state, visitor);
  while (num--) {
    mem_store_->OnATTOp();
  }
}

// Wrapper functions for VersionBuffer
inline uint64_t AddrTransController::VBNewBlock(VersionBuffer& vb) {
  mem_store_->OnBufferOp();
  return vb.NewBlock();
}

inline void AddrTransController::VBFreeBlock(VersionBuffer& vb,
    uint64_t mach_addr, VersionBuffer::State state) {
  mem_store_->OnBufferOp();
  vb.FreeBlock(mach_addr, state);
}

inline void AddrTransController::VBBackupBlock(VersionBuffer& vb,
    uint64_t mach_addr, VersionBuffer::State state) {
  mem_store_->OnBufferOp();
  vb.BackupBlock(mach_addr, state);
}

inline void AddrTransController::VBClearBackup(VersionBuffer& vb) {
  int num = vb.ClearBackup();
  while (num--) {
    mem_store_->OnBufferOp();
  }
}

#endif // SEXAIN_ADDR_TRANS_CONTROLLER_H_

