#include <iostream>
#include <cstdint>
#include "addr_trans_controller.h"

#define KB uint64_t(1024)
#define MB (1024 * KB)
#define GB (1024 * MB)

#define BLOCK_BITS 6
#define PAGE_BITS 12

using namespace std;

inline uint64_t Addr(uint64_t base, int tag, int bits) {
  return base + (tag << bits);
}

void ScanWrite(AddrTransController& atc, uint64_t base,
    int b, int e, int bits) {
  cout << dec << "Scanning writes (" << b << ", " << e << ")" << endl << hex;
  for (uint64_t tag = b; tag < e; ++tag) {
    atc.StoreAddr(Addr(base, tag, bits));
  }
}

void ScanRead(AddrTransController& atc, uint64_t base, int b, int e, int bits) {
  cout << dec << "Scanning reads (" << b << ", " << e << ")" << endl << hex;
  for (uint64_t tag = b; tag < e; ++tag) {
    cout << (Addr(base, tag, bits)) << '\t'
        << atc.LoadAddr(Addr(base, tag, bits)) << endl;
  }
}

int main(int argc, const char* argv[]) {
  const uint64_t k_dram_size = 64 * MB;
  const uint64_t k_nvm_size = GB;
  const int k_blk_tbl_len = 8;
  const int k_pg_tbl_len = 8;

  TraceMemStore mem;

  DirectMapper blk_mapper(k_blk_tbl_len);
  DirectMapper pg_mapper(k_pg_tbl_len);
  AddrTransTable blk_tbl(BLOCK_BITS, blk_mapper, mem);
  AddrTransTable pg_tbl(PAGE_BITS, pg_mapper, mem);

  AddrTransController atc(k_dram_size, k_nvm_size, blk_tbl, pg_tbl);
  
  ScanWrite(atc, 0, 0, 8, PAGE_BITS);
  ScanWrite(atc, k_dram_size, 0, 4, BLOCK_BITS);

  ScanWrite(atc, 0, 8, 12, PAGE_BITS);
  ScanWrite(atc, 0, 4, 8, PAGE_BITS);
  ScanWrite(atc, k_dram_size, 0, 4, BLOCK_BITS);

  ScanRead(atc, 0, 0, 16, PAGE_BITS);
  ScanRead(atc, k_dram_size, 0, 8, BLOCK_BITS);

  return 0;
}
