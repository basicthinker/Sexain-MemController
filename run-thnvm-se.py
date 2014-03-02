#!/bin/bash

GEM5ROOT=~/Projects/Sexain-MemController/gem5-stable
ARCH=X86 #X86_MESI_CMP_directory # in ./build_opts
GEM5=$GEM5ROOT/build/$ARCH/gem5.opt
SE_SCRIPT=$GEM5ROOT/configs/thnvm-se.py

CPU_TYPE=atomic # timing, detailed
NUM_CPUS=1

MEM_TYPE=simple_mem # ddr3_1600_x64
MEM_SIZE=2GB # for whole physical address space
DRAM_SIZE=2GB
ATT_LENGTH=0
MC_PT_LEN=256 # secondary page table length

L1D_SIZE=32kB
L1D_ASSOC=8
L1I_SIZE=32kB
L1I_ASSOC=8

L2_SIZE=256kB
L2_ASSOC=8
NUM_L2CACHES=$NUM_CPUS

L3_SIZE=$((3*NUM_CPUS))MB
L3_ASSOC=30

CPU2006ROOT=~/Share/spec-cpu-2006/benchspec/CPU2006
OUT_DIR=~/Documents/gem5out

while getopts "hc:o:b:t:" opt; do
  case $opt in
    h)
      $GEM5 -h
      $GEM5 $SE_SCRIPT -h
      exit 0
      ;;
    c)
      COMMAND="-c $OPTARG"
      ALIAS=`basename $OPTARG`
      ;;
    o)
      COMMAND="$COMMAND -o $OPTARG"
      ;;
    b)
      COMMAND="--cpu-2006=$OPTARG"
      ALIAS=`basename $OPTARG`
      ;;
    t)
      $GEM5 $SE_SCRIPT --cpu-2006-root=$CPU2006ROOT --check-cpu-2006=$OPTARG
      exit 0
      ;;
    \?)
      exit -1
      ;;
  esac
done

if [ -z $COMMAND ]; then
  echo "Usage: $0 [-h] [-c COMMAND] [-o OPTIONS] [-b|-t BENCHMARK]" >&2
  exit -1
fi

OPTIONS="--caches --l2cache"
OPTIONS+=" --cpu-type=$CPU_TYPE"
OPTIONS+=" --num-cpus=$NUM_CPUS"
OPTIONS+=" --mem-type=$MEM_TYPE"
OPTIONS+=" --mem-size=$MEM_SIZE"
OPTIONS+=" --dram-size=$DRAM_SIZE"
OPTIONS+=" --att-length=$ATT_LENGTH"
OPTIONS+=" --mc-page-table-length=$MC_PT_LEN"
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

$GEM5 -d $OUT_DIR/$ALIAS-`date +%F` $SE_SCRIPT $OPTIONS $COMMAND

