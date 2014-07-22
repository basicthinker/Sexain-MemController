// profiler.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_PROFILER_H_
#define SEXAIN_PROFILER_H_

#include <cstdint>
#include <cassert>

class Profiler {
 public:
  Profiler(int block_bits, int page_bits);
  Profiler(const Profiler& p);

  void AddTableOp(int num = 1) { num_table_ops_ += num; }
  void AddBlockMoveIntra(int num = 1);
  void AddBlockMoveInter(int num = 1);
  void AddPageMoveIntra(int num = 1);
  void AddPageMoveInter(int num = 1);

  uint64_t SumLatency();
  uint64_t SumBusUtil();

  unsigned int num_table_ops() const { return num_table_ops_; }
  uint64_t bytes_intra_channel() const { return bytes_intra_channel_; }
  uint64_t bytes_inter_channel() const { return bytes_inter_channel_; }

  void set_op_latency(int64_t lat) { op_latency_ = lat; }
  void set_exclude_intra(bool b = true) { exclude_intra_ = b; }

  static Profiler Null;
  static Profiler Overlap;

 private:
  const int block_bits_;
  const int page_bits_;

  unsigned int num_table_ops_;
  uint64_t bytes_intra_channel_;
  uint64_t bytes_inter_channel_;

  int64_t op_latency_;
  bool exclude_intra_;
};

inline Profiler::Profiler(int block_bits, int page_bits) :
    block_bits_(block_bits), page_bits_(page_bits),
    num_table_ops_(0), bytes_intra_channel_(0), bytes_inter_channel_(0) {
  op_latency_ = -1;
  exclude_intra_ = false;
}

inline Profiler::Profiler(const Profiler& p) :
    Profiler(p.block_bits_, p.page_bits_) {
  op_latency_ = p.op_latency_;
  exclude_intra_ = p.exclude_intra_;
}

inline void Profiler::AddBlockMoveIntra(int num) {
  bytes_intra_channel_ += num << block_bits_;
}

inline void Profiler::AddBlockMoveInter(int num) {
  bytes_inter_channel_ += num << block_bits_;
}

inline void Profiler::AddPageMoveIntra(int num) {
  bytes_intra_channel_ += num << page_bits_;
}

inline void Profiler::AddPageMoveInter(int num) {
  bytes_inter_channel_ += num << page_bits_;
}

inline uint64_t Profiler::SumLatency() {
  assert(op_latency_ >= 0);
  return op_latency_ * num_table_ops_;
}

inline uint64_t Profiler::SumBusUtil() {
  return (exclude_intra_ ? 0 : bytes_intra_channel_) + bytes_inter_channel_;
}

#endif // SEXAIN_PROFILER_H_
