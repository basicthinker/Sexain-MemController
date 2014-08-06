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
     printf("Usage: %s [SIZE in MBs] [NUM of steps] [WRITE RATIO] [ATT ENTRIES]\n",
         argv[0]);
     return 1;
  }

  const int size = atoi(argv[1]);
  const int step = atoi(argv[2]);
  const int ratio = atoi(argv[3]); // write ratio
  const int entries = atoi(argv[4]);

  const int blocks = M / sizeof(block_t) * size; // per step
  const int swift = entries;
  const size_t bytes = size * M + step * swift * sizeof(block_t);

  printf("size=%d, step=%d, bytes=%lu, blocks=%d, swift=%d\n",
      size, step, bytes, blocks, swift);

  int num = 0;
  block_t* const bmem = (block_t*)malloc(bytes);
  assert(bmem);

  block_t* mem = bmem;
  for (int init = swift; init <= blocks; init += swift) {
    for (int i = 0; i < ratio; ++i) {
      for (int j = 0; j < swift; ++j) {
        mem[j].values[4] = 'R'; 
      }
      for (int j = 0; j < swift; ++j) {
        if (mem[j].values[4] == 'R') {
          ++num;
        }
      }
    }
    mem += swift;
  }
  
  mem = bmem;
  for (int k = 0; k < step; ++k) {
    mem += swift; // do not bother to free
    for (int i = 0; i < ratio; ++i) {
      for (int j = 0; j < blocks; ++j) {
        mem[j].values[4] = 'R'; 
      }
      for (int j = 0; j < blocks; ++j) {
        if (mem[j].values[4] == 'R') {
          ++num;
        }
      }
    }
  }
  
  printf("%d\n", num);
}

