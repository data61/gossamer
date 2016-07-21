#!/bin/sh 
set -x

# Inputs: 
#   1. Program name
GOSS=$1

test ! -d out && mkdir out
test ! -d out/work && mkdir out/work

G=out/work

${GOSS} build-graph -v -k 27 --log-file out/a.log -O ${G}/graph  -I in.fa 
${GOSS} print-contigs -G ${G}/graph -o out/graph-linsegs-contigs.txt 

${GOSS} thread-reads -v --log-file out/a-thread.log -G ${G}/graph -I in.fa  
${GOSS} build-supergraph -v --log-file out/a-supergraph.log -C 1 -G ${G}/graph

${GOSS} thread-pairs -v --log-file out/a-pairs.log -C 1 -G ${G}/graph --insert-expected-size 50 -I p_1.fa -I p_2.fa
${GOSS} print-supergraph-contigs -v -G ${G}/graph -o out/graph-supergraph-contigs.txt --min-length 0 


