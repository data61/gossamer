This tests the bowtie example.
\_____/
/     \

See the out/*.contigs for the output: 
out/graph-linsegs.contigs  out/graph-supergraph.contigs

Dot pdf files are also produced:
out/graph-collapse.pdf  out/graph.pdf  out/graph-supergraph.pdf

inr.fa is the set of contigs in in.fa together with their reverse complements.

Inputs:
in.fa

* bbs 14-11-11
  Found the example didn't work. Firstly, need min length of 50 for unique segments.
  Secondly, the middle part of the bubble was also output as a contig.
