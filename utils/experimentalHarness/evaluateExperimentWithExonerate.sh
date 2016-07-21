#!/bin/bash

GENOMESIZE=$1
GENOME=$2
CONTIGS=$3

query=`tempfile`
cat $CONTIGS | perl filterShortContigs.pl | perl contigLengths.pl > $CONTIGS.rawcontiglens &
perl splitAtN.pl $CONTIGS | perl filterShortContigs.pl > $query &
wait

matches99=`tempfile`

exonerate --showalignment 0 --showvulgar 0 --showsugar 1 --percent 99 -q $query -t $GENOME | awk '{print $2}' | sort | uniq > $matches99 &
contiglengths=${CONTIGS}.contiglens
perl contigLengths.pl $query | sort -k 2 > $contiglengths &
wait
echo "N50 raw:"
perl calculateN50.pl $CONTIGS.rawcontiglens
echo "Cov:"
numerator=`perl sum.pl $contiglengths`
echo "$numerator / $GENOMESIZE"
echo "N50 split:"
perl calculateN50.pl $contiglengths
matchlens99=${CONTIGS}.matchlens99
echo "Length" > $matchlens99
join -2 2 $matches99 $contiglengths | awk '{print $2}' >> $matchlens99 &
wait

echo "SC@99:"
numerator=`perl sum.pl $matchlens99`
echo "$numerator / $GENOMESIZE"
echo "NG50@99:"
perl calculateN50.pl  --ng50 $GENOMESIZE $matchlens99

rm -f $matches99 $query
