#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "uthash.h"

struct entry {
  int key;
  char value[64];
  UT_hash_handle hh;
};

#define K (1024)
#define M (1024 * K)

int main(int argc, const char* argv[]) {
  if (argc != 2) {
    printf("Usage: %s [SIZE in MBs]\n", argv[0]);
    return -1;
  }

  const uint64_t mbs = atol(argv[1]);
  const uint64_t max_size = mbs * M;

  struct entry R;
  const int value_size = sizeof(R.value);
  assert(value_size == 64);
  const int num_keys = max_size / value_size;

  struct entry *store = NULL;
  for (int i = 0; i < value_size; ++i) {
    R.value[i] = 'R';
  }

  for (int i = 0; i < num_keys; ++i) {
    int k = rand() % num_keys;
    struct entry *e;
    HASH_FIND_INT(store, &k, e);
    if (e) {
      assert(strncmp(e->value, R.value, value_size) == 0);
      HASH_DEL(store, e);
      free(e);
    } else {
      struct entry* e = malloc(sizeof(struct entry));
      e->key = k;
      strncpy(e->value, R.value, value_size);
      HASH_ADD_INT(store, key, e);
    }
  }

  printf("Final # entries: %d\n", HASH_COUNT(store));
}

