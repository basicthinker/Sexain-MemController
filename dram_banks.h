// dram_banks.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_DRAM_BANKS_H_
#define SEXAIN_DRAM_BANKS_H_

#include <cstdint>
#include <vector>
#include <cassert>

class DRAMBanks {
 public:

  class Bank {
   public:
    Bank() : open_row_(INVALID_ROW) { }
    uint32_t open_row() const { return open_row_; }
    void set_open_row(uint32_t row) { open_row_ = row; }
    uint64_t busy_time() const { return busy_time_; }
    void set_busy_time(uint64_t time) { busy_time_ = time; }
    static const uint32_t INVALID_ROW = -1;
   private:
    uint32_t open_row_;
    uint64_t busy_time_;
  };

  DRAMBanks(uint64_t capacity, int burst_size, int row_buffer_size,
      int ranks_per_channel, int banks_per_rank);
  bool IsBankBusy(uint64_t addr, uint64_t time);
  Bank* Access(uint64_t addr);
  int row_buffer_size() const { return row_buffer_size_; }

 protected:
  const int burst_size_;
  const int row_buffer_size_;
  const int ranks_per_channel_;
  const int banks_per_rank_;

 private:
  void ParseAddr(uint64_t addr, int& rank, int& bank, int& row);

  uint64_t capacity_;
  int columns_per_row_buffer_;
  int rows_per_bank_;
  std::vector<std::vector<Bank>> banks_;
};

class DDR3Banks : public DRAMBanks {
 public:
  DDR3Banks(uint64_t capacity) : DRAMBanks(capacity, 64, 8192, 2, 8) { }
};

inline DRAMBanks::DRAMBanks(uint64_t capacity, int burst_size,
    int row_buffer_size, int ranks_per_channel, int banks_per_rank) :

    burst_size_(burst_size),
    row_buffer_size_(row_buffer_size), ranks_per_channel_(ranks_per_channel),
    banks_per_rank_(banks_per_rank), capacity_(capacity),
    banks_(ranks_per_channel, std::vector<Bank>(banks_per_rank)) {

  columns_per_row_buffer_ = row_buffer_size_ / burst_size_;
  rows_per_bank_ = capacity / (row_buffer_size_ * banks_per_rank_ *
      ranks_per_channel_);
}

inline void DRAMBanks::ParseAddr(uint64_t addr,
    int& rank, int& bank, int& row) {
  addr = addr / burst_size_;
  addr = addr / columns_per_row_buffer_;

  bank = addr % banks_per_rank_;
  addr = addr / banks_per_rank_;
  assert(bank >= 0 && bank < banks_per_rank_);

  rank = addr % ranks_per_channel_;
  addr = addr / ranks_per_channel_;
  assert(rank >= 0 && rank < ranks_per_channel_);

  row = addr % rows_per_bank_;
  addr = addr / rows_per_bank_;
  assert(row >= 0 && row < rows_per_bank_);
}

inline bool DRAMBanks::IsBankBusy(uint64_t addr, uint64_t time) {
  int rank, bank, row;
  ParseAddr(addr, rank, bank, row);
  return banks_[rank][bank].busy_time() > time;
}

inline DRAMBanks::Bank* DRAMBanks::Access(uint64_t addr) {
  int rank, bank, row;
  ParseAddr(addr, rank, bank, row);
  Bank& b = banks_[rank][bank];
  if (b.open_row() == row) {
    return &b; // assuming the bank structure is kept valid
  } else {
    b.set_open_row(row);
    return NULL;
  }
}

#endif // SEXAIN_DRAM_BANKS_H_

