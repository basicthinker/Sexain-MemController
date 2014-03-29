#!/bin/bash

BENCHMARKS=(
401.bzip2
429.mcf
453.povray
#454.calculix
#456.hmmer
458.sjeng
459.GemsFDTD
#462.libquantum
470.lbm
471.omnetpp
#998.specrand
#999.specrand
)

OUT_FILE=cpu2006-`date +%m%d-%H%M%S`

for benchmark in ${BENCHMARKS[@]}
do
  nohup ./run-thnvm-se.sh -b $benchmark $* >>$OUT_FILE.log 2>>$OUT_FILE.err &
done

