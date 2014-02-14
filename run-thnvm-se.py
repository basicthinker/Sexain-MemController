#!/bin/bash

if [ $# -lt 1 ]; then
  echo "Usage: $0 [-h] [-c COMMAND] [-o OPTIONS] ..."
  exit -1
fi

GEM5ROOT=~/Projects/Sexain-MemController/gem5-stable
ARCH=X86 #X86_MESI_CMP_directory # in ./build_opts

CPU_TYPE=timing # atomic, detailed
NUM_CPUS=1

MEM_TYPE=ddr3_1600_x64 # simple_mem
MEM_SIZE=2GB

L1D_SIZE=32kB
L1D_ASSOC=8
L1I_SIZE=32kB
L1I_ASSOC=8

L2_SIZE=256kB
L2_ASSOC=8
NUM_L2CACHES=$NUM_CPUS

L3_SIZE=$((3*NUM_CPUS))MB
L3_ASSOC=30

CPU2006ROOT=~/Documents/spec-cpu-2006/benchspec/CPU2006

OPTIONS="--caches --l2cache"
OPTIONS+=" --cpu-type=$CPU_TYPE"
OPTIONS+=" --num-cpus=$NUM_CPUS"
OPTIONS+=" --mem-type=$MEM_TYPE"
OPTIONS+=" --mem-size=$MEM_SIZE"
OPTIONS+=" --l1d_size=$L1D_SIZE"
OPTIONS+=" --l1d_assoc=$L1D_ASSOC"
OPTIONS+=" --l1i_size=$L1I_SIZE"
OPTIONS+=" --l1i_assoc=$L1I_ASSOC"
OPTIONS+=" --l2_size=$L2_SIZE"
OPTIONS+=" --l2_assoc=$L2_ASSOC"
OPTIONS+=" --num-l2caches=$NUM_L2CACHES"
OPTIONS+=" --l3_size=$L3_SIZE"
OPTIONS+=" --l3_assoc=$L3_ASSOC"
OPTIONS+=" --cpu-2006-root=$CPU2006ROOT"
$GEM5ROOT/build/$ARCH/gem5.opt $GEM5ROOT/configs/thnvm-se.py $OPTIONS $@
