// index_queue.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "index_queue.h"

using namespace std;
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

  --length_;
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

  ++length_;
}

void IndexQueue::Accept(QueueVisitor* visitor) {
  int i = Front();
  int tmp;
  while (i != -EINVAL) {
    tmp = array_[i].second;
    visitor->Visit(i);
    i = tmp;
  }
}

