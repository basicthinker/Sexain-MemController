#!/bin/bash

#!/bin/bash

if [ $# -lt 1 ]; then
  echo "Usage: $0 [POSTFIX] [PATTERN]"
  exit 1
fi

post=$1
pattern=$2

stats=~/Projects/Sexain-MemController/scripts/list-stats.py
root=~/Documents

dirs=($root/gem5out-a0-p0-d0GB-$post \
    $root/gem5out-a0-p0-d2GB-$post \
    $root/gem5out-a6144-p4128-d0GB-$post \
    $root/gem5out-a6144-p4128-d2GB-$post \
    $root/gem5out-a256-d16MB-$post \
    $root/gem5out-a512-d16MB-$post \
    $root/gem5out-a1024-d16MB-$post \
    $root/gem5out-a2048-d16MB-$post \
    $root/gem5out-a4096-d16MB-$post \
    $root/gem5out-a8192-d16MB-$post \
    $root/gem5out-a2048-d4MB-$post \
    $root/gem5out-a2048-d8MB-$post \
    $root/gem5out-a2048-d32MB-$post \
    $root/gem5out-a2048-d64MB-$post)

for dir in ${dirs[@]}; do
  $stats -d $dir -r "$pattern" -s sim_seconds system.mem_ctrls.total_ckpt_time \
      system.mem_ctrls.bytes_written::writebacks \
      system.mem_ctrls.bytes_channel \
      system.mem_ctrls.readRowHits system.mem_ctrls.readRowMisses \
      system.mem_ctrls.writeRowHits system.mem_ctrls.writeRowMisses \
      system.l3cache.cache_flushes system.mem_ctrls.num_epochs
done

for dir in ${dirs[@]}; do
  $stats -d $dir -r "$pattern" -p system.mem_ctrls -s \
      total_wait_time bytes_inter_channel \
      att_write_hits att_write_misses num_nvm_writes \
      num_dram_writes avg_nvm_dirty_ratio avg_dram_write_ratio \
      avg_pages_to_dram avg_pages_to_nvm
done

