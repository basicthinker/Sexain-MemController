#!/bin/bash

BENCHMARKS=(
401.bzip2
429.mcf
453.povray
454.calculix
456.hmmer
458.sjeng
459.GemsFDTD
462.libquantum
470.lbm
471.omnetpp
998.specrand
999.specrand
)

for benchmark in ${BENCHMARKS[@]}
do
  nohup ./run-thnvm-se.py -b $benchmark >cpu2006.log 2>cpu2006.err &
done

