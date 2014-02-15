// index_queue.h
// Copyright (c) 2014 Jinglei Ren <jinglei.ren@stanzax.org>

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
  bool Empty();
  void Remove(int i);
  int PopFront();
  void PushBack(int i);
  void PushFront(int i);
  std::set<int> Indexes() const;
 private:
  IndexNode& FrontNode();
  IndexNode& BackNode();
  void SetFront(int i) { head_.first = i; }
  void SetBack(int i) { head_.second = i; }

  IndexNode head_;
  IndexArray& array_;
};

std::set<int> IndexIntersection(const IndexQueue& a, const IndexQueue& b);

IndexQueue::IndexQueue(IndexArray& arr) : array_(arr) {
  SetFront(-EINVAL);
  SetBack(-EINVAL);
}

IndexNode& IndexQueue::FrontNode() {
  assert(Front() >= 0);
  return array_[Front()];
}

IndexNode& IndexQueue::BackNode() {
  assert(Back() >= 0);
  return array_[Back()];
}

bool IndexQueue::Empty() {
  assert((Front() == -EINVAL) == (Back() == -EINVAL));
  return Front() == -EINVAL;
}

void IndexQueue::Remove(int i) {
  assert(i >= 0);
  const int prev = array_[i].first;
  const int next = array_[i].second;

  if (prev == -EINVAL) {
    assert(Front() == i);
    SetFront(next);
  } else {
    array_[prev].second = next;
  }

  if (next == -EINVAL) {
    assert(Back() == i);
    SetBack(prev);
  } else {
    array_[next].first = prev;
  }

  array_[i].first = -EINVAL;
  array_[i].second = -EINVAL;
}

int IndexQueue::PopFront() {
  if (Empty()) return -EINVAL;
  const int front = Front();
  Remove(front);
  return front;
}

void IndexQueue::PushBack(int i) {
  if (Empty()) {
    array_[i].first = array_[i].second = -EINVAL;
    SetFront(i);
    SetBack(i);
  } else {
    array_[i].first = Back();
    array_[i].second = -EINVAL;
    BackNode().second = i;
    SetBack(i);
  }
}

void IndexQueue::PushFront(int i) {
  if (Empty()) {
    array_[i].first = array_[i].second = -EINVAL;
    SetFront(i);
    SetBack(i);
  } else {
    array_[i].first = -EINVAL;
    array_[i].second = Front();
    FrontNode().first = i;
    SetFront(i);
  }
}

std::set<int> IndexQueue::Indexes() const {
  std::set<int> indexes;
  for (int i = Front(); i != -EINVAL; i = array_[i].second) {
    bool ret = indexes.insert(i).second;
    assert(ret);
  }
  return indexes;
}

std::set<int> IndexIntersection(const IndexQueue& a, const IndexQueue& b) {
  std::set<int> ai = a.Indexes();
  std::set<int> bi = b.Indexes();
  std::set<int> common;
  set_intersection(ai.begin(), ai.end(), bi.begin(), bi.end(),
      std::inserter(common, common.begin()));
  return common;
}

#endif // SEXAIN_INDEX_QUEUE_H_
