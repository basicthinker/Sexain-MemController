// dram_banks.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_DRAM_BANKS_H_
#define SEXAIN_DRAM_BANKS_H_

#include <cstdint>
#include <vector>
#include <cassert>

typedef uint64_t Time;

class DRAMBanks {
 public:

  struct Address {
    int rank_ID;
    int bank_ID;
    int row_ID;
  };

  class Bank {
   public:
    Bank() : open_row_(-EINVAL), busy_time_(0) { }
    uint32_t open_row() const { return open_row_; }
    Time busy_time() const { return busy_time_; }
    void set_busy_time(Time time) { busy_time_ = time; }

    bool Access(int row);
    void PushWrite(Address addr);
    Address PopWrite();
    bool HasPendingWrite() const { return !write_queue_.empty(); }

   private:
    uint32_t open_row_;
    Time busy_time_;
    std::vector<Address> write_queue_;
  };

  DRAMBanks(uint64_t capacity, int burst_size, int row_buffer_size,
      int ranks_per_channel, int banks_per_rank);
  void PushWrite(uint64_t addr);
  bool HasPendingWrite() const;
  Bank* Access(uint64_t addr, bool& hit);
  Time NextTime(Time t) const;
  /// Process pending checkpoint blocks.
  std::vector<std::pair<DRAMBanks::Bank*, bool>> Access(Time t, int* flushed);
  int row_buffer_size() const { return row_buffer_size_; }

 protected:
  const int burst_size_;
  const int row_buffer_size_;
  const int ranks_per_channel_;
  const int banks_per_rank_;

 private:
  Address ParseAddr(uint64_t addr);
  Bank* Access(Address addr, bool& hit);

  uint64_t capacity_;
  int columns_per_row_buffer_;
  int rows_per_bank_;
  std::vector<std::vector<Bank>> banks_;
};

class DDR3Banks : public DRAMBanks {
 public:
  DDR3Banks(uint64_t capacity) : DRAMBanks(capacity, 64, 8192, 2, 8) { }
};

inline bool DRAMBanks::Bank::Access(int row) {
  return open_row() == row ? true : (open_row_ = row, false);
}

inline void DRAMBanks::Bank::PushWrite(Address addr) {
  assert(write_queue_.empty() || write_queue_.back().bank_ID == addr.bank_ID);
  write_queue_.push_back(addr);
}

inline DRAMBanks::Address DRAMBanks::Bank::PopWrite() {
  Address addr = write_queue_.back();
  write_queue_.pop_back();
  return addr;
}

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

inline DRAMBanks::Address DRAMBanks::ParseAddr(uint64_t addr) {
  Address a;
  addr = addr / burst_size_;
  addr = addr / columns_per_row_buffer_;

  a.bank_ID = addr % banks_per_rank_;
  addr = addr / banks_per_rank_;
  assert(a.bank_ID >= 0 && a.bank_ID < banks_per_rank_);

  a.rank_ID = addr % ranks_per_channel_;
  addr = addr / ranks_per_channel_;
  assert(a.rank_ID >= 0 && a.rank_ID < ranks_per_channel_);

  a.row_ID = addr % rows_per_bank_;
  addr = addr / rows_per_bank_;
  assert(a.row_ID >= 0 && a.row_ID < rows_per_bank_);
  return a;
}

inline void DRAMBanks::PushWrite(uint64_t addr) {
  Address a = ParseAddr(addr);
  banks_[a.rank_ID][a.bank_ID].PushWrite(a);
}

inline bool DRAMBanks::HasPendingWrite() const {
  for (int i = 0; i < banks_.size(); ++i) {
    for (int j = 0; j < banks_[i].size(); ++j) {
      if (banks_[i][j].HasPendingWrite()) return true;
    }
  }
  return false;
}

inline DRAMBanks::Bank* DRAMBanks::Access(DRAMBanks::Address addr, bool& hit) {
  Bank& bank = banks_[addr.rank_ID][addr.bank_ID];
  hit = bank.Access(addr.row_ID);
  return &bank;
}

inline DRAMBanks::Bank* DRAMBanks::Access(uint64_t addr, bool& hit) {
  return Access(ParseAddr(addr), hit);
}

inline std::vector<std::pair<DRAMBanks::Bank*, bool>> DRAMBanks::Access(
    Time t, int* flushed) {
  std::vector<std::pair<DRAMBanks::Bank*, bool>> ret;
  *flushed = 0;
  for (int i = 0; i < banks_.size(); ++i) {
    for (int j = 0; j < banks_[i].size(); ++j) {
      if (banks_[i][j].busy_time() == t) ++(*flushed);
      if (banks_[i][j].busy_time() <= t && banks_[i][j].HasPendingWrite()) {
        bool hit;
        Bank* bank = Access(banks_[i][j].PopWrite(), hit);
        ret.push_back(std::make_pair(bank, hit));
      }
    }
  }
  return ret;
}

inline Time DRAMBanks::NextTime(Time now) const {
  Time next = -1; // max integer value
  for (int i = 0; i < banks_.size(); ++i) {
    for (int j = 0; j < banks_[i].size(); ++j) {
      if (banks_[i][j].busy_time() > now && banks_[i][j].busy_time() < next)
        next = banks_[i][j].busy_time();
    }
  }
  return next == -1 ? 0 : next;
}

#endif // SEXAIN_DRAM_BANKS_H_
