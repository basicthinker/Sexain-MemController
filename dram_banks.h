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
    static const uint32_t INVALID_ROW = -1;
   private:
    uint32_t open_row_;
  };

  DRAMBanks(uint64_t capacity, int burst_size, int row_buffer_size,
      int ranks_per_channel, int banks_per_rank);
  bool access(uint64_t addr);

 protected:
  uint64_t capacity_;
  const int burst_size_;
  const int row_buffer_size_;
  const int ranks_per_channel_;
  const int banks_per_rank_;

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

    capacity_(capacity), burst_size_(burst_size),
    row_buffer_size_(row_buffer_size), ranks_per_channel_(ranks_per_channel),
    banks_per_rank_(banks_per_rank),
    banks_(ranks_per_channel, std::vector<Bank>(banks_per_rank)) {

  columns_per_row_buffer_ = row_buffer_size_ / burst_size_;
  rows_per_bank_ = capacity / (row_buffer_size_ * banks_per_rank_ *
      ranks_per_channel_);
}

inline bool DRAMBanks::access(uint64_t addr) {
  addr = addr / burst_size_;
  addr = addr / columns_per_row_buffer_;

  int bank = addr % banks_per_rank_;
  addr = addr / banks_per_rank_;
  assert(bank >= 0 && bank < banks_per_rank_);

  int rank = addr % ranks_per_channel_;
  addr = addr / ranks_per_channel_;
  assert(rank >= 0 && rank < ranks_per_channel_);

  int row = addr % rows_per_bank_;
  addr = addr / rows_per_bank_;
  assert(row >= 0 && row < rows_per_bank_);

  Bank& b = banks_[rank][bank];
  if (b.open_row() == row) {
    return true;
  } else {
    b.set_open_row(row);
    return false;
  }
}

#endif // SEXAIN_DRAM_BANKS_H_

