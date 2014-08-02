#!/bin/bash

#!/bin/bash

if [ $# != 1 ]; then
  echo "Usage: #0 [POSTFIX]"
  exit 1
fi

post=$1

stats=~/Projects/Sexain-MemController/scripts/list-stats.py
root=~/Documents

simple_dirs=($root/gem5out-a0-p0-d0GB-$post \
    $root/gem5out-a0-p0-d2GB-$post \
    $root/gem5out-a6144-p4128-d0GB-$post \
    $root/gem5out-a6144-p4128-d2GB-$post)

for dir in ${simple_dirs[@]}; do
  $stats -d $dir -s sim_seconds system.mem_ctrls.total_ckpt_time \
      system.mem_ctrls.bytes_written::writebacks \
      system.mem_ctrls.bytes_channel \
      system.l3cache.cache_flushes system.mem_ctrls.num_epochs
done

$stats -d $root/gem5out-a2048-d16MB-$post -s sim_seconds \
    system.mem_ctrls.total_ckpt_time system.mem_ctrls.total_wait_time

$stats -d $root/gem5out-a2048-d16MB-$post -p system.mem_ctrls -s \
    bytes_written::writebacks bytes_channel bytes_inter_channel num_epochs \
    att_write_hits att_write_misses num_nvm_writes \
    num_dram_writes avg_nvm_dirty_ratio avg_dram_write_ratio \
    avg_pages_to_dram avg_pages_to_nvm

