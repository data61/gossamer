#!/bin/bash

SRC=allpathsCor
K=55
TRIM=0
DATADIR=../data/$SRC/
OUTDIR=./out
GOSS=../../bin/goss-unlimNs

mkdir -p $OUTDIR

FRAG_1=$DATADIR/frag_1.fastq
FRAG_2=$DATADIR/frag_2.fastq
SHORTJUMP_1=$DATADIR/shortjump_1.fastq
SHORTJUMP_2=$DATADIR/shortjump_2.fastq

logMemory()
{
        LOGFILE=$1
        PID=$2
        echo "Waiting for pid $2"
        while ps -l $2; do sleep 5; done | grep -v "SZ" | awk '{print $10}' | sort -rn | head -n 1 > $LOGFILE
}

date > ${OUTDIR}/date.begin

$GOSS build-graph -k $K -O $OUTDIR/graph -i $FRAG_1 -i $FRAG_2 -i $SHORTJUMP_1 -i $SHORTJUMP_2 -B 4 -T 8 -v -l $OUTDIR/build-graph.log &
logMemory ${OUTDIR}/build-graph.mem $!
wait

$GOSS trim-graph -G $OUTDIR/graph -O $OUTDIR/graph-t1 -C $TRIM -v -l $OUTDIR/trim-graph.log &
logMemory ${OUTDIR}/trim-graph.mem $!
wait

$GOSS prune-tips -G $OUTDIR/graph-t1 -O $OUTDIR/graph-t1-p4 --iterate 4 -T 8 -v -l $OUTDIR/prune-tips.log &
logMemory ${OUTDIR}/prune-tips.mem $!
wait

$GOSS pop-bubbles -G $OUTDIR/graph-t1-p4 -O $OUTDIR/graph-t1-p4-b1 -T 8 -v -l $OUTDIR/pop-bubbles.log &
logMemory ${OUTDIR}/pop-bubbles.mem $!
wait

#$GOSS print-contigs -G $OUTDIR/graph-t1-p4-b1 -o $OUTDIR/contigs-1.fa --verbose-headers --min-length 100 -v -l $OUTDIR/print-contigs-1.log &
#logMemory ${OUTDIR}/print-contigs-1.mem $!
#wait

$GOSS build-entry-edge-set -G $OUTDIR/graph-t1-p4-b1 -T 8 -v -l $OUTDIR/build-entry-edge-set.log &
logMemory ${OUTDIR}/build-entry-edge-set.mem $!
wait

$GOSS build-supergraph -G $OUTDIR/graph-t1-p4-b1 -v -l $OUTDIR/build-supergraph.log &
logMemory ${OUTDIR}/build-supergraph.mem $!
wait

$GOSS thread-pairs -G $OUTDIR/graph-t1-p4-b1 -T 8 --innies -i $FRAG_1 -i $FRAG_2 --insert-expected-size 160 --expected-coverage 30 --min-link-count 4 -v -l $OUTDIR/thread-pairs-frag.log &
logMemory ${OUTDIR}/thread-pairs-frag.mem $!
wait

#$GOSS print-contigs -G $OUTDIR/graph-t1-p4-b1 -o $OUTDIR/contigs-2.fa --verbose-headers --min-length 100 -v -l $OUTDIR/print-contigs-2.log &
#logMemory ${OUTDIR}/print-contigs-2.mem $!
#wait

$GOSS thread-pairs -G $OUTDIR/graph-t1-p4-b1 -T 8 --mate-pairs -i $SHORTJUMP_1 -i $SHORTJUMP_2 --insert-expected-size 3500 --expected-coverage 30 --min-link-count 4 -v -l $OUTDIR/thread-pairs-shortjump.log &
logMemory ${OUTDIR}/thread-pairs-shortjump.mem $!
wait

$GOSS print-contigs -G $OUTDIR/graph-t1-p4-b1 -o $OUTDIR/contigs-3.fa --verbose-headers --min-length 100 -v -l $OUTDIR/print-contigs-3.log &
logMemory ${OUTDIR}/print-contigs3.mem $!
wait
ln -s $OUTDIR/contigs-3.fa genome.ctg.fa


#$GOSS build-scaffold -G $OUTDIR/graph-t1-p4-b1 -T 8 -i $FRAG_1 -i $FRAG_2 --insert-expected-size 160 --expected-coverage 40 --scaffold-out $OUTDIR/scaf -v -D show-stats 2>$OUTDIR/build-scaffold-frag.log

#mkdir -p $OUTDIR/sg_tp
#cp $OUTDIR/graph-t1-p4-b1*supergraph* $OUTDIR/sg_tp

$GOSS build-scaffold -G $OUTDIR/graph-t1-p4-b1 -T 8 --mate-pairs -i $SHORTJUMP_1 -i $SHORTJUMP_2 --insert-expected-size 3500 --expected-coverage 30 --scaffold-out $OUTDIR/scaf -v -D show-stats 2>$OUTDIR/build-scaffold-shortjump.log &
logMemory ${OUTDIR}/build-scaffold-shortjump.mem $!
wait

$GOSS scaffold -G $OUTDIR/graph-t1-p4-b1 -T 8 --scaffold-in $OUTDIR/scaf --min-link-count 4 -v 2>$OUTDIR/scaffold.log &
logMemory ${OUTDIR}/scaffold.mem $!
wait

$GOSS print-contigs -G $OUTDIR/graph-t1-p4-b1 -o $OUTDIR/contigs-4.fa --verbose-headers --min-length 100 -v -l $OUTDIR/print-contigs-4.log &
logMemory ${OUTDIR}/print-contigs-4.mem $!
wait
ln -s $OUTDIR/contigs-4.fa genome.scf.fa

date > ${OUTDIR}/date.end

chmod -R a-w ${OUTDIR}/*
