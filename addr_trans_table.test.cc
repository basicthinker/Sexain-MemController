#include <iostream>
#include "addr_trans_table.h"

#define CACHE_BLOCK_BITS 6

using namespace std;

inline uint64_t TestAddr(uint64_t tag) {
  return (tag << CACHE_BLOCK_BITS) + 1;
}

void ScanWrite(AddrTransTable& att, int b, int e) {
  cout << dec << "Scanning writes (" << b << ", " << e << ")" << endl << hex;
  for (uint64_t tag = b; tag < e; tag += 1) {
    if (!att.Probe(TestAddr(tag))) att.NewEpoch();
    att.StoreAddr(TestAddr(tag));
  }
}

void ScanRead(AddrTransTable& att, int b, int e) {
  cout << dec << "Scanning reads (" << b << ", " << e << ")" << endl << hex;
  for (uint64_t tag = b; tag < e; tag += 1) {
    cout << (TestAddr(tag)) << '\t'
        << att.LoadAddr(TestAddr(tag)) << endl;
  }
}

int main(int argc, const char* argv[]) {
  DirectMapper mapper(8);
  TraceMemStore mem;
  AddrTransTable att(CACHE_BLOCK_BITS, mapper, mem);
  att.set_image_floor(0x1000);

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

