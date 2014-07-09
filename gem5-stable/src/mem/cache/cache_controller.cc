// cache_controller.cc
// Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>

#include "mem/cache/cache_controller.h"

CacheController* CacheControllerParams::create() {
  return new CacheController(this);
}

