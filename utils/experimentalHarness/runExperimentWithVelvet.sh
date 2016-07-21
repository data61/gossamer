#!/bin/bash

GENOME=../NC_009800.fna

TEST=cd070rl100er0.60s1


wait

#!/bin/bash
GENOME=../NC_009800.fna

VELVETH=/home/genome/ajb/src/velvet_1.1.04/velveth
VELVETG=/home/genome/ajb/src/velvet_1.1.04/velvetg

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

	echo "Test $TEST k=$K"

	date
	echo "velveth pair"
	$VELVETH ${DIR}/pair $K -fastq.gz -shortPaired ../${TEST}_interleave.fq.gz > ${DIR}/vh_pair.log 2>&1 &
        logMemory ${DIR}/vh_pair.mem $!
	wait

	date
	echo "velvetg pair"
	$VELVETG ${DIR}/pair -cov_cutoff ${COVCUTOFF} -scaffolding no -exp_cov $EXPCOV  > ${DIR}/vg_pair.log 2>&1 &
        logMemory ${DIR}/vg_pair.mem $!
	wait

#	date
#	echo "velveth scaff"
#	$VELVETH ${DIR}/scaff $K -fastq.gz -shortPaired ../${TEST}_interleave.fq.gz > ${DIR}/vh_pair.log 2>&1 &
#        logMemory ${DIR}/vh_scaff.mem $!
#	wait

#	date
#	echo "velvetg scaff"
#	$VELVETG ${DIR}/scaff -cov_cutoff 7 -scaffolding yes -exp_cov $EXPCOV  > ${DIR}/vg_pair.log 2>&1 &
#        logMemory ${DIR}/vg_scaff.mem $!
#	wait

#	date
#	echo "velveth single"
#	$VELVETH ${DIR}/single $K -fastq.gz -short ../${TEST}_interleave.fq.gz > ${DIR}/vh_single.log 2>&1 &
#        logMemory ${DIR}/vh_single.mem $!
#	wait

#	date
#	echo "velvetg single"
#	$VELVETG ${DIR}/single -exp_cov $EXPCOV  > ${DIR}/vg_single.log 2>&1 &
#        logMemory ${DIR}/vg_single.mem $!
#	wait

	date
	echo "Test done"

	#perl ../../contigLengths.pl ${DIR}/pair/contigs.fa > ${DIR}/pair-contigs.txt
	#perl ./contigLengths.pl ${DIR}/single/contigs.fa > ${DIR}/single-contigs.txt

	wait
}


runTest ERR022075_21p 505 55 11 100
runTest cd100rl100er0.60s1 505 55 6 700
runTest ERR022075 505 55 20 100

