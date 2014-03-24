// index_queue.h
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#ifndef SEXAIN_INDEX_QUEUE_H_
#define SEXAIN_INDEX_QUEUE_H_

#include <cerrno>
#include <cassert>
#include <set>
#include <algorithm>

typedef std::pair<int, int> IndexNode;

class IndexArray {
 public:
  virtual IndexNode& operator[](int i) = 0;
};

class IndexQueue {
 public:
  IndexQueue(IndexArray& arr);
  int Front() const { return head_.first; }
  int Back() const { return head_.second; }
  bool Empty() const;
  void Remove(int i);
  int PopFront();
  void PushBack(int i);
  void PushFront(int i);
  std::set<int> Indexes() const;
  int GetLength() const;
 private:
  IndexNode& FrontNode();
  IndexNode& BackNode();
  void SetFront(int i) { head_.first = i; }
  void SetBack(int i) { head_.second = i; }

  IndexNode head_;
  IndexArray& array_;
};

inline IndexQueue::IndexQueue(IndexArray& arr) : array_(arr) {
  SetFront(-EINVAL);
  SetBack(-EINVAL);
}

inline IndexNode& IndexQueue::FrontNode() {
  assert(Front() >= 0);
  return array_[Front()];
}

inline IndexNode& IndexQueue::BackNode() {
  assert(Back() >= 0);
  return array_[Back()];
}

inline bool IndexQueue::Empty() const {
  assert((Front() == -EINVAL) == (Back() == -EINVAL));
  return Front() == -EINVAL;
}

inline int IndexQueue::PopFront() {
  if (Empty()) return -EINVAL;
  const int front = Front();
  Remove(front);
  return front;
}

inline std::set<int> IndexQueue::Indexes() const {
  std::set<int> indexes;
  for (int i = Front(); i != -EINVAL; i = array_[i].second) {
    bool ret = indexes.insert(i).second;
    assert(ret);
  }
  return indexes;
}

inline int IndexQueue::GetLength() const {
  int len = 0;
  for (int i = Front(); i != -EINVAL; i = array_[i].second) {
    ++len;
  }
  return len;
}

inline std::set<int> IndexIntersection(const IndexQueue& a,
      const IndexQueue& b) {
  std::set<int> ai = a.Indexes();
  std::set<int> bi = b.Indexes();
  std::set<int> common;
  set_intersection(ai.begin(), ai.end(), bi.begin(), bi.end(),
      std::inserter(common, common.begin()));
  return common;
}

#endif // SEXAIN_INDEX_QUEUE_H_

