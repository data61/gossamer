#!/bin/bash
GENOME=../NC_009800.fna

GOSS=/home/genome/ajb/work/gossamer/src/bin/gcc-4.4.3/release-popcnt/debug-symbols-on/link-static/runtime-link-static/threading-multi/goss

logMemory()
{
	LOGFILE=$1
	PID=$2
        echo "Waiting for pid $2"
	while ps -l $2; do sleep 5; done | grep -v "SZ" | awk '{print $10}' | sort -rn | head -n 1 > $LOGFILE
}

runTest()
{
	TEST=$1
	INSERT_SIZE=$2
	K=$3
	COVCUTOFF=$4
	EXPCOV=$5

	DIR="${TEST}k${K}"

	if test -d ${DIR}; then
	    rm -rf ${DIR}
	fi

	if test ! -d ${DIR}; then
	    mkdir ${DIR}
	fi

	echo "Test $TEST with k=$K"
	date
	echo "Build graph"
	$GOSS build-graph -i ../${TEST}_1.fq.gz -i ../${TEST}_2.fq.gz -O ${DIR}/graph -v -k $K -B 12 -T 8 --log-file ${DIR}/graph.log &
        logMemory ${DIR}/graph.mem $!
	wait
	date
	echo "Trim graph"
	$GOSS trim-graph -v -G ${DIR}/graph -O ${DIR}/graph-trim --log-file ${DIR}/graph-trim.log -C ${COVCUTOFF} -T 8 &
        logMemory ${DIR}/graph-trim.mem $!
	wait
	date
	echo "Prune tips 1"
	$GOSS prune-tips -v -G ${DIR}/graph-trim -O ${DIR}/graph-trim-p1 --log-file ${DIR}/graph-trim-p1.log -T 8 &
        logMemory ${DIR}/graph-trim-p1.mem $!
	wait
	date
	echo "Prune tips 2"
	$GOSS prune-tips -v -G ${DIR}/graph-trim-p1 -O ${DIR}/graph-trim-p2 --log-file ${DIR}/graph-trim-p2.log -T 8 &
        logMemory ${DIR}/graph-trim-p2.mem $!
	wait
	date
	echo "Prune tips 3"
	$GOSS prune-tips -v -G ${DIR}/graph-trim-p2 -O ${DIR}/graph-trim-p3 --log-file ${DIR}/graph-trim-p3.log -T 8 &
        logMemory ${DIR}/graph-trim-p3.mem $!
	wait
	date
	echo "Prune tips 4"
	$GOSS prune-tips -v -G ${DIR}/graph-trim-p3 -O ${DIR}/graph-trim-p4 --log-file ${DIR}/graph-trim-p4.log -T 8 &
        logMemory ${DIR}/graph-trim-p4.mem $!
	wait
	date
	echo "Pop bubbles"
	$GOSS pop-bubbles -v -G ${DIR}/graph-trim-p4 -O ${DIR}/graph-trim-p4-popb --log-file ${DIR}/graph-trim-p4-popb.log -T 8 &
        logMemory ${DIR}/graph-trim-p4-popb.mem $!
	wait

#	echo "Trim paths"
#	$GOSS trim-paths -v -G ${DIR}/graph-trim-p4-popb -O ${DIR}/graph-trim-p4-popb-trim -C $TRIM --log-file ${DIR}/graph-trim-p4-popb-trim.log -T 8 &
#        logMemory ${DIR}/graph-trim-p4-popb-trim.mem $!
#	wait

	date
	echo "Entry edge sets"
	date
	$GOSS build-entry-edge-set -v -G ${DIR}/graph-trim-p4-popb -T 8 --log-file ${DIR}/build-entry-edge-set.log &
        logMemory ${DIR}/build-entry-edge-set.mem $!
	wait
#	date
#	echo "Read sets"
#	$GOSS build-read-sets -G ${DIR}/graph-trim-p4-popb -D build-empty-sets --log-file ${DIR}/build-read-sets.log &
#	#$GOSS build-read-sets -G ${DIR}/graph-trim-p4-popb -i ../${DIR}_1.fq.gz -i ../${DIR}_2.fq.gz
#        logMemory ${DIR}/build-read-sets.mem $!
#	wait
	date
	echo "Supergraph"
	$GOSS build-supergraph -v -G ${DIR}/graph-trim-p4-popb -T 8 --log-file ${DIR}/build-supergraph.log &
        logMemory ${DIR}/build-supergraph.mem $!
	wait
	date
	echo "Thread pairs"
	$GOSS thread-pairs -v -G ${DIR}/graph-trim-p4-popb -T 8 -i ../${TEST}_1.fq.gz -i ../${TEST}_2.fq.gz --insert-expected-size ${INSERT_SIZE} --expected-coverage $EXPCOV --search-radius 10 --log-file ${DIR}/thread-pairs.log &
	#$GOSS thread-reads -v -G ${DIR}/graph-trim-p4-popb -T 8 
        logMemory ${DIR}/thread-pairs.mem $!
	wait
	date
	echo "Done"

	$GOSS print-contigs -G ${DIR}/graph-trim-p4-popb --min-length 100 -o ${DIR}/pairs-contigs.fa -T 8
	#perl ./contigLengths.pl ${DIR}/pairs-contigs.fa > ${DIR}/pairs-contigs.txt

	wait
}


runTest ERR022075_21p 505 55 11 100
runTest cd100rl100er0.60s1 505 55 6 100
runTest ERR022075     505 55 20 700

