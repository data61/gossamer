#!/bin/bash

GENOMESIZE=$1
GENOME=$2
CONTIGS=$3

query=`tempfile`
perl filterShortContigs.pl $CONTIGS | perl contigLengths.pl > $CONTIGS.rawcontiglens &
perl splitAtN.pl $CONTIGS | perl filterShortContigs.pl > $query &
wait

matches=`tempfile`
#matches=matches.file

bwa bwasw -t 2 $GENOME $query | perl matchAlignments.pl | sort | uniq > $matches &
contiglengths=${CONTIGS}.contiglens
#contiglengths=contiglengths.file
perl contigLengths.pl $query | sort -k 2 > $contiglengths &
wait
echo "N50 raw:"
perl calculateN50.pl $CONTIGS.rawcontiglens
echo "Cov:"
numerator=`perl sum.pl $contiglengths`
echo "$numerator / $GENOMESIZE"
echo "N50 split:"
perl calculateN50.pl $contiglengths
matchlens=${CONTIGS}.matchlens
#matchlens=matchlens.file
join -2 2 $matches $contiglengths | awk '{print $2}' > $matchlens
echo "SC:"
numerator=`perl sum.pl $matchlens`
echo "$numerator / $GENOMESIZE"
echo "NG50@99:"
perl calculateN50.pl  --ng50 $GENOMESIZE $matchlens

rm -f $matches $query
