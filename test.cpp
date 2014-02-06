#include <vector>
#include <iostream>
#include <algorithm>
#include <iterator>
#include "index_queue.h"

#define CACHE_BLOCK_BITS 6
#define PAGE_BLOCK_BITS 12

using namespace std;

class Array : public IndexArray {
 public:
  Array(int n) : array_(n) { }
  IndexNode& operator[](int i) { return array_[i]; }
 private:
  vector<IndexNode> array_;
};

void Print(const IndexQueue& q, const char* title) {
  vector<int> indexes = q.Indexes();
  cout << title;
  copy(indexes.begin(), indexes.end(), ostream_iterator<int>(cout, " "));
  cout << endl;
}

int main(int argc, const char* argv[]) {
  int n = 10;
  Array arr(10);
  IndexQueue cq(arr);
  IndexQueue dq(arr);

  for (int i = 0; i < n; ++i) {
    dq.PushBack(i);
  }
  Print(cq, "Clean: ");
  Print(dq, "Dirty: ");
  cout << endl;

  for (int i = 0; i < n/2; ++i) {
    dq.Remove(i);
    dq.PushFront(i);
  }
  Print(cq, "Clean: ");
  Print(dq, "Dirty: ");
  cout << endl;

  for (int i = n/3; i < n; ++i) {
    dq.Remove(i);
    cq.PushBack(i);
  }
  Print(cq, "Clean: ");
  Print(dq, "Dirty: ");
  cout << endl;

  for (int i = 0; i < n/3; ++i) {
    cq.PushBack(dq.PopFront());
  }
  Print(cq, "Clean: ");
  Print(dq, "Dirty: ");
  cout << endl;

  return 0;
}
