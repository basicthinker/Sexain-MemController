#!/bin/bash

GEM5ROOT=~/Projects/Sexain-MemController/gem5-stable
ARCH=X86 #X86_MESI_CMP_directory # in ./build_opts
GEM5=$GEM5ROOT/build/$ARCH/gem5.opt
SE_SCRIPT=$GEM5ROOT/configs/thnvm-se.py

CPU_TYPE=timing # atomic, detailed
NUM_CPUS=1
CPU_CLOCK=3GHz

MEM_TYPE=simple_mem # ddr3_1600_x64
MEM_SIZE=2GB # for whole physical address space
DRAM_SIZE=128MB
ATT_LEN=1024
PTT_LEN=256 # secondary page table length

L1D_SIZE=32kB
L1D_ASSOC=8
L1I_SIZE=32kB
L1I_ASSOC=8

L2_SIZE=256kB
L2_ASSOC=8

L3_SIZE=$((3*NUM_CPUS))MB
L3_ASSOC=24

CPU2006ROOT=~/Share/spec-cpu-2006/benchspec/CPU2006
OUT_DIR=~/Documents/gem5out-a$ATT_LEN-p$PTT_LEN-d$MEM_SIZE
BUILD_NAME=build_base_none.0000

to_run=0
to_test=0

while getopts "hc:o:b:g:a:t:l" opt; do
  case $opt in
    h)
      $GEM5 -h
      $GEM5 $SE_SCRIPT -h
      ;;
    c)
      COMMAND="-c $OPTARG"
      ALIAS=`basename $OPTARG`
      to_run=1
      ;;
    o)
      COMMAND+=" -o $OPTARG"
      ;;
    b)
      COMMAND="--cpu-2006=$OPTARG"
      ALIAS=$OPTARG
      to_run=1
      to_test=1
      ;;
    l)
      OUT_DIR+="-lat"
      OPTIONS+=" --att-latency"
      ;;
    t)
      ALIAS=$OPTARG
      to_test=1
      ;;
    g)
      GEM5OPT+=" $OPTARG"
      to_run=1
      ;;
    a)
      OPTIONS+=" $OPTARG"
      ;;
    \?)
      exit -1
      ;;
  esac
done

OUT_DIR+='-'`date +%m%d`

OPTIONS+=" --caches --l2cache --l3cache"
OPTIONS+=" --cpu-type=$CPU_TYPE"
OPTIONS+=" --num-cpus=$NUM_CPUS"
OPTIONS+=" --cpu-clock=$CPU_CLOCK"
OPTIONS+=" --mem-type=$MEM_TYPE"
OPTIONS+=" --mem-size=$MEM_SIZE"
OPTIONS+=" --dram-size=$DRAM_SIZE"
OPTIONS+=" --att-length=$ATT_LEN"
OPTIONS+=" --ptt-length=$PTT_LEN"
OPTIONS+=" --l1d_size=$L1D_SIZE"
OPTIONS+=" --l1d_assoc=$L1D_ASSOC"
OPTIONS+=" --l1i_size=$L1I_SIZE"
OPTIONS+=" --l1i_assoc=$L1I_ASSOC"
OPTIONS+=" --l2_size=$L2_SIZE"
OPTIONS+=" --l2_assoc=$L2_ASSOC"
OPTIONS+=" --l3_size=$L3_SIZE"
OPTIONS+=" --l3_assoc=$L3_ASSOC"
OPTIONS+=" --cpu-2006-root=$CPU2006ROOT"
OPTIONS+=" --cpu-2006-build-name=$BUILD_NAME"

if [ $to_run = 1 ]; then
  $GEM5 -d $OUT_DIR/$ALIAS $GEM5OPT $SE_SCRIPT $OPTIONS $COMMAND
fi

if [ $? -eq 0 ] && [ $to_test = 1 ]; then
  $GEM5 $SE_SCRIPT $OPTIONS --check-cpu-2006=$ALIAS 1>&2
fi

