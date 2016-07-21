#!/bin/bash

SRC=quakeCor
K=45
TRIM=2
DATADIR=../data/$SRC/
OUTDIR=./out
GOSS=../../bin/goss-unlimNs
THREADS=8

mkdir -p $OUTDIR

FRAG_1=$DATADIR/frag_1.fastq
FRAG_2=$DATADIR/frag_2.fastq
SHORTJUMP_1=$DATADIR/shortjump_1.fastq
SHORTJUMP_2=$DATADIR/shortjump_2.fastq
MEDIUMJUMP_1=$DATADIR/mediumjump_1.fastq
MEDIUMJUMP_2=$DATADIR/mediumjump_2.fastq

logMemory()
{
        LOGFILE=$1
        PID=$2
        echo "Waiting for pid $2"
        while ps -l $2; do sleep 5; done | grep -v "SZ" | awk '{print $10}' | sort -rn | head -n 1 > $LOGFILE
}

date > ${OUTDIR}/date.begin

$GOSS build-graph -k $K -O $OUTDIR/graph -i $FRAG_1 -i $FRAG_2 -i $SHORTJUMP_1 -i $SHORTJUMP_2 -i $MEDIUMJUMP_1 -i $MEDIUMJUMP_2 -B 24 -T $THREADS -v -l $OUTDIR/build-graph.log &
logMemory ${OUTDIR}/build-graph.mem $!
wait

$GOSS trim-graph -G $OUTDIR/graph -O $OUTDIR/graph-t1 -C $TRIM -v -l $OUTDIR/trim-graph.log &
logMemory ${OUTDIR}/trim-graph.mem $!
wait

$GOSS prune-tips -G $OUTDIR/graph-t1 -O $OUTDIR/graph-t1-p4 --iterate 4 -T $THREADS -v -l $OUTDIR/prune-tips.log &
logMemory ${OUTDIR}/prune-tips.mem $!
wait

$GOSS pop-bubbles -G $OUTDIR/graph-t1-p4 -O $OUTDIR/graph-t1-p4-b1 -T $THREADS -v -l $OUTDIR/pop-bubbles.log &
logMemory ${OUTDIR}/pop-bubbles.mem $!
wait

#$GOSS print-contigs -G $OUTDIR/graph-t1-p4-b1 -o $OUTDIR/contigs-1.fa --verbose-headers --min-length 100 -v -l $OUTDIR/print-contigs-1.log &
#logMemory ${OUTDIR}/print-contigs-1.mem $!
#wait

$GOSS build-entry-edge-set -G $OUTDIR/graph-t1-p4-b1 -T $THREADS -v -l $OUTDIR/build-entry-edge-set.log &
logMemory ${OUTDIR}/build-entry-edge-set.mem $!
wait

$GOSS build-supergraph -G $OUTDIR/graph-t1-p4-b1 -v -l $OUTDIR/build-supergraph.log &
logMemory ${OUTDIR}/build-supergraph.mem $!
wait

$GOSS thread-pairs -G $OUTDIR/graph-t1-p4-b1 -T $THREADS --innies -i $FRAG_1 -i $FRAG_2 --insert-expected-size 400 --expected-coverage 60 --min-link-count 2 -v -D show-link-stats -D save-link-map -D show-dist-stats 2>$OUTDIR/thread-pairs-frag.log &
logMemory ${OUTDIR}/thread-pairs-frag.mem $!
wait

#$GOSS print-contigs -G $OUTDIR/graph-t1-p4-b1 -T $THREADS -o $OUTDIR/contigs-2.fa --verbose-headers --min-length 100 -v 2>$OUTDIR/print-contigs-2.log &
#logMemory ${OUTDIR}/print-contigs-2.mem $!
#wait

$GOSS thread-pairs -G $OUTDIR/graph-t1-p4-b1 -T $THREADS --outies -i $SHORTJUMP_1 -i $SHORTJUMP_2 --insert-expected-size 3500 --expected-coverage 60 --min-link-count 2 -v 2>$OUTDIR/thread-pairs-shortjump.log &
logMemory ${OUTDIR}/thread-pairs-shortjump.mem $!
wait

#$GOSS print-contigs -G $OUTDIR/graph-t1-p4-b1 -T $THREADS -o $OUTDIR/contigs-3.fa --verbose-headers --min-length 100 -v -l $OUTDIR/print-contigs-3.log &
#logMemory ${OUTDIR}/print-contigs-3.mem $!
#wait

$GOSS thread-pairs -G $OUTDIR/graph-t1-p4-b1 -T $THREADS --outies -i $MEDIUMJUMP_1 -i $MEDIUMJUMP_2 --insert-expected-size 8000 --expected-coverage 60 --min-link-count 2 -D show-dist-stats -v 2>$OUTDIR/thread-pairs-mediumjump.log &
logMemory ${OUTDIR}/thread-pairs-mediumjump.mem $!
wait

$GOSS print-contigs -G $OUTDIR/graph-t1-p4-b1 -T $THREADS -o $OUTDIR/contigs-4.fa --verbose-headers --min-length 100 -v -l $OUTDIR/print-contigs-4.log &
logMemory ${OUTDIR}/print-contigs-4.mem $!
wait
ln -s $OUTDIR/contigs-4.fa genome.ctg.fa

# $GOSS build-scaffold -G $OUTDIR/graph-t1-p4-b1 -T $THREADS -i $FRAG_1 -i $FRAG_2 --insert-expected-size 400 --expected-coverage 60 --scaffold-out $OUTDIR/scaf -v -l $OUTDIR/build-scaffold-frag.log

#mkdir -p $OUTDIR/sg_tp
#cp $OUTDIR/graph-t1-p4-b1*supergraph* $OUTDIR/sg_tp

$GOSS build-scaffold -G $OUTDIR/graph-t1-p4-b1 -T $THREADS --outies -i $FRAG_1 -i $FRAG_2 --insert-expected-size 400 --expected-coverage 60 --scaffold-out $OUTDIR/scaf -v -l $OUTDIR/build-scaffold-frag.log &
logMemory ${OUTDIR}/build-scaffold-frag.mem $!
wait

$GOSS build-scaffold -G $OUTDIR/graph-t1-p4-b1 -T $THREADS --outies -i $SHORTJUMP_1 -i $SHORTJUMP_2 --insert-expected-size 3500 --expected-coverage 60 --scaffold-out $OUTDIR/scaf -v -l $OUTDIR/build-scaffold-shortjump.log &
logMemory ${OUTDIR}/build-scaffold-shortjump.mem $!
wait

$GOSS build-scaffold -G $OUTDIR/graph-t1-p4-b1 -T $THREADS --outies -i $MEDIUMJUMP_1 -i $MEDIUMJUMP_2 --insert-expected-size 8000 --expected-coverage 60 --scaffold-out $OUTDIR/scaf -v -l $OUTDIR/build-scaffold-mediumjump.log &
logMemory ${OUTDIR}/build-scaffold-mediumjump.mem $!
wait

$GOSS scaffold -G $OUTDIR/graph-t1-p4-b1 -T $THREADS --min-link-count 10 --scaffold-in $OUTDIR/scaf -v -l $OUTDIR/scaffold.log &
logMemory ${OUTDIR}/scaffold.mem $!
wait

$GOSS print-contigs -G $OUTDIR/graph-t1-p4-b1 -T $THREADS -o $OUTDIR/contigs-5.fa --verbose-headers --min-length 100 -v -l $OUTDIR/print-contigs-5.log &
logMemory ${OUTDIR}/print-contigs-5.mem $!
wait
ln -s $OUTDIR/contigs-5.fa genome.scf.fa

date > ${OUTDIR}/date.end

chmod -R a-w ${OUTDIR}/*
