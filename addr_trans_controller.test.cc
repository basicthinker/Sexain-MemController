#include <iostream>
#include <cstdint>
#include "addr_trans_controller.h"

#define KB uint64_t(1024)
#define MB (1024 * KB)
#define GB (1024 * MB)

#define BLOCK_BITS 6
#define PAGE_BITS 12

using namespace std;

int main(int argc, const char* argv[]) {
  const uint64_t k_dram_size = 512 * MB;
  const uint64_t k_nvm_size = GB;
  const int k_blk_tbl_len = 1024;
  const int k_pg_tbl_len = 1024;

  TraceMemStore mem;

  DirectMapper blk_mapper(k_blk_tbl_len);
  DirectMapper pg_mapper(k_pg_tbl_len);
  AddrTransTable blk_tbl(BLOCK_BITS, blk_mapper, mem);
  AddrTransTable pg_tbl(PAGE_BITS, pg_mapper, mem);

  AddrTransController controller(k_dram_size, k_nvm_size, blk_tbl, pg_tbl);
  
  return 0;
}
