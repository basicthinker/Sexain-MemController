// array_strm.c
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define K (1024)
#define M (1024 * K)

typedef struct block {
  uint64_t values[8]; // 64 bytes
} block_t;

int main(int argc, const char* argv[]) {
  if (argc != 2) {
     printf("Usage: %s [SIZE in MBs]\n", argv[0]);
     return 1;
  }

  const unsigned int size = atoi(argv[1]);

  const size_t bytes = size * M;
  const int blocks = bytes / sizeof(block_t); // per step

  printf("size=%d, bytes=%lu, blocks=%d\n",
      size, bytes, blocks);

  int num = 0;
  block_t* mem = (block_t*)malloc(bytes);
  assert(mem);
  for (int i = 0; i < blocks; ++i) {
    int off = rand() % blocks;
    if (mem[off].values[4] == 'R') {
      ++num;
    }
    mem[i].values[4] = 'R';
  }
  printf("%d\n", num);
}

