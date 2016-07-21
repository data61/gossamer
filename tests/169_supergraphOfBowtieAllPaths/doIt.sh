#!/bin/sh
set -x

# Defaults:
# in.fa
#
# Inputs: 
#   1. Program name
GOSS=$1

test ! -d out && mkdir out

${GOSS} build-graph -v -k 27 -S 28 --log-file out/a.log -O out/graph  -I in.fa 
${GOSS} print-contigs -G out/graph -o out/graph-linsegs-contigs.txt 

# Duplicate the reads.
${GOSS} thread-reads -v --log-file out/a-thread.log -G out/graph -I in.fa -I in.fa  -I in.fa  -I in.fa 
${GOSS} build-supergraph -v --log-file out/a-supergraph.log -G out/graph --min-coverage 1
${GOSS} print-supergraph-contigs -v -G out/graph -o out/graph-supergraph-contigs.txt --min-length 0 

${GOSS} dot-graph -G out/graph -o out/graph.dot -I out/graph-supergraph-contigs.txt  --show-counts  --label-edges  
${GOSS} dot-graph -G out/graph -o out/graph-collapse.dot -I out/graph-supergraph-contigs.txt  --show-counts --collapse-linear-paths   --label-edges 
${GOSS} dot-supergraph -G out/graph -o out/graph-supergraph.dot
dot -Tpdf out/graph.dot -o out/graph.pdf
dot -Tpdf out/graph-collapse.dot -o out/graph-collapse.pdf
dot -Tpdf out/graph-supergraph.dot -o out/graph-supergraph.pdf

