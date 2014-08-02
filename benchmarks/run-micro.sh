#!/bin/bash

if [ $# != 1 ]; then
  echo "Usage: #0 [FACTOR]"
  exit 1
fi

factor=$1

RD=$((32 * factor))
ST=$((96 * factor))
SL=$((3 * factor))

bench_dir=~/Projects/Sexain-MemController/benchmarks
programs=($bench_dir/array_rand.o \
    $bench_dir/array_strm.o \
    $bench_dir/array_slid.o)
args=("16 $RD 49152" "$ST" "16 $SL 2 2048")

simulators=($bench_dir/run-scripts/run-DRAM.sh \
    $bench_dir/run-scripts/run-NVM.sh \
    $bench_dir/run-scripts/run-journal.sh \
    $bench_dir/run-scripts/run-shadow.sh \
    $bench_dir/run-scripts/run-thnvm.sh)

for simu in ${simulators[@]}; do
  for ((i=0; i<${#programs[@]}; ++i)); do
    $simu -c ${programs[i]} -o "${args[i]}" &
    sleep 0.2
  done
done

