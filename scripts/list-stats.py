#!/usr/bin/python

import argparse
import os
import sys

parser = argparse.ArgumentParser()
parser.add_argument("-d", "--dirs", nargs='*', help="list of gem5out dirs")
parser.add_argument("-s", "--stats", nargs='*', help="list of statistic items")

args = parser.parse_args()

if args.dirs is None:
    print >> sys.stderr, "[Error] No target gem5out dirs are specified!"
    exit(-1)

results = { } # results[benchmark][dir][stat]

for dir in args.dirs:
    for bench in os.listdir(dir):
        if bench not in results:
            results[bench] = { }
        results[bench][dir] = { }

        stats_file = open(dir + '/' + bench + '/stats.txt', 'r')
        for line in stats_file:
            items = line.split()
            if len(items) >= 2 and items[0] in args.stats:
                results[bench][dir][items[0]] = items[1]

# Header
for dir in args.dirs:
    items = os.path.split(dir)
    if len(items[-1]) == 0:
        items = os.path.split(items[-2])
    sys.stdout.write('\t' + items[-1])
print

for bench in results.keys():
    sys.stdout.write(bench)
    for dir in args.dirs:
        for stat in args.stats:
            if stat in results[bench][dir]:
                sys.stdout.write('\t' + results[bench][dir][stat])
            else:
                sys.stdout.write('\t')
    print    

