// index_queue.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "index_queue.h"

using namespace std;

void IndexQueue::Remove(int i) {
  assert(i >= 0);
  const int prev = array_[i].prev;
  const int next = array_[i].next;

  if (prev == -EINVAL) {
    assert(Front() == i);
    SetFront(next);
  } else {
    array_[prev].next = next;
  }

  if (next == -EINVAL) {
    assert(Back() == i);
    SetBack(prev);
  } else {
    array_[next].prev = prev;
  }

  array_[i].prev = -EINVAL;
  array_[i].next = -EINVAL;

  --length_;
}

void IndexQueue::PushBack(int i) {
  assert(i >= 0);
  assert(array_[i].prev == -EINVAL && array_[i].next == -EINVAL);
  if (Empty()) {
    SetFront(i);
    SetBack(i);
  } else {
    array_[i].prev = Back();
    BackNode().next = i;
    SetBack(i);
  }

  ++length_;
}

void IndexQueue::Accept(QueueVisitor* visitor) {
  int i = Front();
  int tmp;
  while (i != -EINVAL) {
    tmp = array_[i].next;
    visitor->Visit(i);
    i = tmp;
  }
}

