#include <cstdint>
#include <cassert>
#include <unordered_map>
#include <string>
#include <iostream>

using namespace std;

int main(int argc, const char* argv[]) {
  if (argc != 3) {
    cout << "Usage: " << argv[0] << " [MAX SIZE] [VALUE SIZE]" << endl;
    return -1;
  }

  const uint64_t max_size = atol(argv[1]);
  const int value_size = atoi(argv[2]);
  const int num_keys = max_size / value_size;
  assert(num_keys > 1024);

  unordered_map<int, string> store;

  for (int i = 0; i < num_keys; ++i) {
    int key = rand() % num_keys;
    unordered_map<int, string>::iterator it = store.find(key);
    if (it != store.end()) {
      assert(it->second.capacity() == value_size);
      assert(it->second.compare(string(value_size, key % 256)) == 0);
      store.erase(it);
    } else {
      store[key] = string(value_size, key % 256);
    }
  }

  cout << "Final # entries: " << store.size() << endl;
}

