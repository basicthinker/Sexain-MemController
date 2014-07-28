// array_rand.c
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
  if (argc != 5) {
     printf("Usage: %s [SIZE in MBs] [NUM of steps] [FACTOR] [ATT ENTRIES]\n",
         argv[0]);
     return 1;
  }

  const unsigned int size = atoi(argv[1]);
  const unsigned int step = atoi(argv[2]);
  const unsigned int factor = atoi(argv[3]); // 1 / write ratio
  const unsigned int entries = atoi(argv[4]);

  const int blocks = size * M / sizeof(block_t); // per step
  const int writes = blocks * factor; // multiplied by expected ratio
  const int swift = entries * blocks / writes;
  const size_t bytes = size * M + step * swift * sizeof(block_t);

  printf("size=%d, step=%d, bytes=%lu, blocks=%d, writes=%d, swift=%d\n",
      size, step, bytes, blocks, writes, swift);

  int num = 0;
  block_t* mem = (block_t*)malloc(bytes);
  assert(mem);
  for (int i = 0; i < step; ++i) {
    for (int j = 0; j < writes; ++j) {
      int off = rand() % blocks;
      if (mem[off].values[4] == 'R') {
        ++num;
      }
      off = rand() % blocks;
      mem[off].values[4] = 'R';
    }
    mem += swift; // do not bother to free
  }
  printf("%d\n", num);
}

