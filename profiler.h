// profiler.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_PROFILER_H_
#define SEXAIN_PROFILER_H_

class Profiler {
 public:
  Profiler(int block_bits, int page_bits);

  void AddTableOp(int num = 1) { num_table_ops_ += num; }
  void AddBlockMoveIntra(int num = 1) { num_block_move_intra_ += num; }
  void AddBlockMoveInter(int num = 1) { num_block_move_inter_ += num; }
  void AddPageMoveIntra(int num = 1);
  void AddPageMoveInter(int num = 1);

  unsigned int num_table_ops() const { return num_table_ops_; }  
  unsigned int num_block_move_intra() const { return num_block_move_intra_; }  
  unsigned int num_block_move_inter() const { return num_block_move_inter_; }  

 private:
  const int block_bits_;
  const int page_bits_;
  const int page_blocks_;
  unsigned int num_table_ops_;
  unsigned int num_block_move_intra_;
  unsigned int num_block_move_inter_;
};

inline Profiler::Profiler(int block_bits, int page_bits) :
    block_bits_(block_bits), page_bits_(page_bits),
    page_blocks_(1 << (page_bits - block_bits)),
    num_table_ops_(0), num_block_move_intra_(0), num_block_move_inter_(0) {
}

inline void Profiler::AddPageMoveIntra(int num) {
  num_block_move_intra_ += num * page_blocks_;
}

inline void Profiler::AddPageMoveInter(int num) {
  num_block_move_inter_ += num * page_blocks_;
}

#endif // SEXAIN_PROFILER_H_

