#!/bin/bash
GENOME=../NC_009800.fna

SOAPDENOVO="/home/genome/ajb/src/SOAPdenovo-V1.05/bin/SOAPdenovo-63mer"

logMemory()
{
	LOGFILE=$1
	PID=$2
	while ps -l $2; do sleep 5; done | grep -v "SZ" | awk '{print $10}' | sort -rn | head -n 1 > $LOGFILE
}

runTest()
{
	TEST=$1
	INSERT_SIZE=$2
	RDLEN=$3
	K=$4

 	DIR="${TEST}k${K}"

	if test -d ${DIR}; then
	    rm -rf ${DIR}
	fi

	if test ! -d ${DIR}; then
	    mkdir ${DIR}
	fi

	echo "max_rd_len=$RDLEN" >> ${DIR}/graph.config
	echo "maplen=80" >> ${DIR}/graph.config
	echo "" >> ${DIR}/graph.config
	echo "[LIB]" >> ${DIR}/graph.config
	echo "avg_ins=$INSERT_SIZE" >> ${DIR}/graph.config
	echo "reverse_seq=0" >> ${DIR}/graph.config
	echo "asm_flags=3" >> ${DIR}/graph.config
	echo "rank=1" >> ${DIR}/graph.config
	echo "q1=../${TEST}_1.fq.gz" >> ${DIR}/graph.config
	echo "q2=../${TEST}_2.fq.gz" >> ${DIR}/graph.config


	echo "Test $TEST k=$K"
	date
        echo "Pregraph"
	$SOAPDENOVO pregraph -s ${DIR}/graph.config -d yes -p 8 -K $K -o ${DIR}/graph -R yes -D yes > ${DIR}/pregraph.log 2>&1 &
        logMemory ${DIR}/pregraph.mem $!
	wait
	date
        echo "Contig"
	$SOAPDENOVO contig -g ${DIR}/graph -M 1 -R yes > ${DIR}/contig.log 2>&1 &
        logMemory ${DIR}/contig.mem $!
	wait
	date
        echo "Map"
	$SOAPDENOVO map -s ${DIR}/graph.config -g ${DIR}/graph -p 8 > ${DIR}/map.log 2>&1 &
        logMemory ${DIR}/map.mem $!
	wait
	date
        echo "Scaffold"
	$SOAPDENOVO scaff -p 8 -g ${DIR}/graph -L 100 > ${DIR}/scaff.log 2>&1 &
        logMemory ${DIR}/scaff.mem $!
	wait

	date
	echo "Test done"

	perl ../../contigLengths.pl ${DIR}/graph.contig > ${DIR}/pair-contigs.txt
#	perl ../../contigLengths.pl ${DIR}/graph.scafSeq > ${DIR}/scaff-contigs.txt

	wait
}


runTest ERR022075_21p 505 100 55
runTest cd100rl100er0.60s1 505 100 55
runTest ERR022075 505 100 55

