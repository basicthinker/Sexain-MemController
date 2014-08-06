#include"red_black_tree.h"
#include<stdio.h>
#include<ctype.h>

#include <stdint.h>
#include <string.h>
#include <assert.h>

#define K (1024)
#define M (1024 * K)

struct Entry {
  int key;
  char* value;
};

void Destroy(void* e) {
  free(((struct Entry*)e)->value);
  free(e);
}

int Comp(const void* a, const void* b) {
  struct Entry* ea = (struct Entry*)a;
  struct Entry* eb = (struct Entry*)b;
  if (ea->key > eb->key) return 1;
  if (ea->key < eb->key) return -1;
  return 0;
}

void ConstNull(const void* a) {
  ;
}

void Null(void* a) {
  ;
}

int main(int argc, const char* argv[]) {
  if (argc != 3) {
    printf("Usage: %s [SIZE in MBs] [VALUE SIZE]\n", argv[0]);
    return -1;
  }

  const uint64_t mbs = atol(argv[1]);
  const uint64_t max_size = mbs * M;
  const int value_size = atoi(argv[2]);
  const int num_keys = max_size / value_size;

  rb_red_blk_tree* tree = RBTreeCreate(Comp, Destroy, Null, ConstNull, Null);
  char* const R = (char*)malloc(value_size);
  for (int i = 0; i < value_size; ++i) {
    R[i] = 'R';
  }

  for (int i = 0; i < num_keys; ++i) {
    int key = rand() % num_keys;
    rb_red_blk_node* node = RBExactQuery(tree, &key);
    if (node) {
      struct Entry* e = (struct Entry*)node->key; 
      assert(strncmp(e->value, R, value_size) == 0);
      RBDelete(tree, node);
    } else {
      char* new_value = (char*)malloc(value_size);
      struct Entry* e = (struct Entry*)malloc(sizeof(struct Entry));
      e->key = key;
      e->value = new_value;
      strncpy(e->value, R, value_size);
      RBTreeInsert(tree, e, 0);
    }
  }
}

