#include <iostream>
#include "addr_trans_table.h"

#define CACHE_BLOCK_BITS 6

using namespace std;

const uint64_t g_step = 1 << CACHE_BLOCK_BITS;

void ScanWrite(AddrTransTable& att, int b, int e) {
  cout << dec << "Scanning writes (" << b << ", " << e << ")" << endl << hex;
  for (uint64_t addr = b * g_step; addr < e * g_step; addr += g_step) {
    att.StoreAddr(addr);
  }
}

void ScanRead(AddrTransTable& att, int b, int e) {
  cout << dec << "Scanning reads (" << b << ", " << e << ")" << endl << hex;
  for (uint64_t addr = b * g_step; addr < e * g_step; addr += g_step) {
    uint64_t mach_addr = att.LoadAddr(addr);
    cout << addr << '\t' << mach_addr;
    if (addr != mach_addr) cout << '*';
    cout << endl;
  }
}

int main(int argc, const char* argv[]) {
  DirectMapper mapper(8, 1024);
  TraceMemStore mem;
  AddrTransTable att(8, CACHE_BLOCK_BITS, mapper, mem);

  ScanWrite(att, 0, 8);
  ScanRead(att, 0, 16);

  ScanWrite(att, 8, 12);
  ScanWrite(att, 0, 4);
  ScanRead(att, 0, 16);

  ScanWrite(att, 0, 4);
  ScanWrite(att, 12, 16);
  ScanRead(att, 0, 16);

  ScanWrite(att, 0, 4);
  ScanRead(att, 0, 16);

  return 0;
}
