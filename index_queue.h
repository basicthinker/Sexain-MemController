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

class QueueVisitor {
 public:
  virtual void Visit(int i) = 0;
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

  void Accept(QueueVisitor* visitor);
  int length() const { return length_; }
 private:
  IndexNode& FrontNode();
  IndexNode& BackNode();
  void SetFront(int i) { head_.first = i; }
  void SetBack(int i) { head_.second = i; }

  IndexNode head_;
  IndexArray& array_;
  int length_;
};

inline IndexQueue::IndexQueue(IndexArray& arr) : array_(arr) {
  SetFront(-EINVAL);
  SetBack(-EINVAL);
  length_ = 0;
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
  assert((length_ == 0) == (Front() == -EINVAL));
  return Front() == -EINVAL;
}

inline int IndexQueue::PopFront() {
  if (Empty()) return -EINVAL;
  const int front = Front();
  Remove(front);
  return front;
}

#endif // SEXAIN_INDEX_QUEUE_H_

