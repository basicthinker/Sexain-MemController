#include <cstdint>
#include <cassert>
#include <cstring>
#include <map>
#include <iostream>

using namespace std;

#define K (1024)
#define M (1024 * K)

int main(int argc, const char* argv[]) {
  if (argc != 3) {
    cout << "Usage: " << argv[0] << " [SIZE in MBs] [VALUE SIZE]" << endl;
    return -1;
  }

  const uint64_t mbs = atol(argv[1]);
  const uint64_t max_size = mbs * M;
  const int value_size = atoi(argv[2]);
  const int num_keys = max_size / value_size;

  map<int, char*> store;
  char* const R = new char[value_size];
  for (int i = 0; i < value_size; ++i) {
    R[i] = 'R';
  }

  for (int i = 0; i < num_keys; ++i) {
    int key = rand() % num_keys;
    map<int, char*>::iterator it = store.find(key);
    if (it != store.end()) {
      assert(strncmp(it->second, R, value_size) == 0);
      delete[] it->second;
      store.erase(it);
    } else {
      char* new_value = new char[value_size];
      store[key] = new_value;
      strncpy(new_value, R, value_size);
    }
  }

  cout << "Final # entries: " << store.size() << endl;
}

