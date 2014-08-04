#!/bin/bash

bench_dir=~/Projects/Sexain-MemController/benchmarks
programs=($bench_dir/hash_table.o $bench_dir/rb_tree.o)
args=("32 16" "32 64" "64 256" "64 1024" "128 4096")

simulators=($bench_dir/run-scripts/run-DRAM.sh \
    $bench_dir/run-scripts/run-NVM.sh \
    $bench_dir/run-scripts/run-journal.sh \
    $bench_dir/run-scripts/run-shadow.sh \
    $bench_dir/run-scripts/run-thnvm.sh)

for simu in ${simulators[@]}; do
  for prog in ${programs[@]}; do
    for ((i=0; i<${#args[@]}; ++i)); do
      $simu -c $prog -o "${args[i]}" &
      sleep 0.5
    done
  done
done

