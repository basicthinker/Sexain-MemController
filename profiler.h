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
  void AddBlockMoveIntra(int num = 1) { num_block_move_intra_ += num; }
  void AddBlockMoveInter(int num = 1) { num_block_move_inter_ += num; }
  void AddPageMoveIntra(int num = 1);
  void AddPageMoveInter(int num = 1);

  uint64_t SumLatency();
  uint64_t SumBusUtil();

  unsigned int num_table_ops() const { return num_table_ops_; }  
  unsigned int num_block_move_intra() const { return num_block_move_intra_; }  
  unsigned int num_block_move_inter() const { return num_block_move_inter_; }  

  void set_op_latency(int64_t lat) { op_latency_ = lat; }
  void set_bus_util_intra_(int64_t util) { bus_util_intra_ = util; }
  void set_bus_util_inter_(int64_t util) { bus_util_inter_ = util; }

  static Profiler Null;
  static Profiler Overlap;

 private:
  const int block_bits_;
  const int page_bits_;
  const int page_blocks_;

  unsigned int num_table_ops_;
  unsigned int num_block_move_intra_;
  unsigned int num_block_move_inter_;

  int64_t op_latency_;
  int64_t bus_util_intra_;
  int64_t bus_util_inter_;
};

inline Profiler::Profiler(int block_bits, int page_bits) :
    block_bits_(block_bits), page_bits_(page_bits),
    page_blocks_(1 << (page_bits - block_bits)),
    num_table_ops_(0), num_block_move_intra_(0), num_block_move_inter_(0) {
  op_latency_ = -1;
  bus_util_intra_ = -1;
  bus_util_inter_ = -1;
}

inline Profiler::Profiler(const Profiler& p) :
    Profiler(p.block_bits_, p.page_bits_) {
  op_latency_ = p.op_latency_;
  bus_util_intra_ = p.bus_util_intra_;
  bus_util_inter_ = p.bus_util_inter_;
}

inline void Profiler::AddPageMoveIntra(int num) {
  num_block_move_intra_ += num * page_blocks_;
}

inline void Profiler::AddPageMoveInter(int num) {
  num_block_move_inter_ += num * page_blocks_;
}

inline uint64_t Profiler::SumLatency() {
  assert(op_latency_ >= 0);
  return op_latency_ * num_table_ops_;
}

inline uint64_t Profiler::SumBusUtil() {
  assert(bus_util_intra_ >= 0 && bus_util_inter_ >= 0);
  return bus_util_intra_ * num_block_move_intra_ +
      bus_util_inter_ * num_block_move_inter_;
}

#endif // SEXAIN_PROFILER_H_
