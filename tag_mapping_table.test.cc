#include <iostream>
#include "tag_mapping_table.h"

#define CACHE_BLOCK_BITS 6

using namespace std;

void ScanWrite(TagMappingTable& tmt, int b, int e) {
  cout << dec << "Scanning writes (" << b << ", " << e << ")" << endl << hex;
  for (uint64_t tag = b; tag < e; tag += 1) {
    tmt.StoreTag(tag);
  }
}

void ScanRead(TagMappingTable& tmt, int b, int e) {
  cout << dec << "Scanning reads (" << b << ", " << e << ")" << endl << hex;
  for (uint64_t tag = b; tag < e; tag += 1) {
    cout << tag << '\t' << tmt.LoadTag(tag) << endl;
  }
}

int main(int argc, const char* argv[]) {
  DirectMapper mapper(8, 1024);
  TraceMemStore mem;
  TagMappingTable tmt(8, CACHE_BLOCK_BITS, mapper, mem);

  ScanWrite(tmt, 0, 8);
  ScanRead(tmt, 0, 16);

  ScanWrite(tmt, 8, 12);
  ScanWrite(tmt, 0, 4);
  ScanRead(tmt, 0, 16);

  ScanWrite(tmt, 0, 4);
  ScanWrite(tmt, 12, 16);
  ScanRead(tmt, 0, 16);

  ScanWrite(tmt, 0, 4);
  ScanRead(tmt, 0, 16);

  return 0;
}

