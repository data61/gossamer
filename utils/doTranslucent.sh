
TEST=$1
THREADS=8
TRANSLUCENT=translucent
EXT=fq

runOneK()
{
    K=$1
    echo "Running with K=${K}"
    date
    $TRANSLUCENT build-graph -v -k ${K} -i ${TEST}_1.${EXT} -i ${TEST}_2.${EXT} -O goss/${TEST}-${K} -T ${THREADS}
    $TRANSLUCENT trim-graph -v -C 2 -G goss/${TEST}-${K} -O goss/${TEST}-${K}-trim
    $TRANSLUCENT prune-tips -v --relative-cutoff 0.1 -G goss/${TEST}-${K}-trim -O goss/${TEST}-${K}-prune
    $TRANSLUCENT pop-bubbles -v --relative-cutoff 0.1 -G goss/${TEST}-${K}-prune -O goss/${TEST}-${K}-clean
    $TRANSLUCENT assemble -v -G goss/${TEST}-${K}-clean -o goss/${TEST}-${K}-assemble.fa -T ${THREADS} -i ${TEST}_1.${EXT} -i ${TEST}_2.${EXT}
}

runOneK 31
runOneK 29
runOneK 27
runOneK 25

#runOneK 19
#runOneK 21
#runOneK 23

echo "Building merged graph"
date

$TRANSLUCENT build-graph -v -k 31 -T ${THREADS} \
#    -I goss/${TEST}-19-assemble.fa \
#    -I goss/${TEST}-21-assemble.fa \
#    -I goss/${TEST}-23-assemble.fa \
    -I goss/${TEST}-25-assemble.fa \
    -I goss/${TEST}-27-assemble.fa \
    -I goss/${TEST}-29-assemble.fa \
    -I goss/${TEST}-31-assemble.fa \
    -O goss/${TEST}-transcripts

$TRANSLUCENT merge-graph-with-reference -v -T ${THREADS} \
    -G goss/${TEST}-transcripts \
    --graph-ref goss/${TEST}-31 \
    -O goss/${TEST}-merge

echo "Final assembly"
date

$TRANSLUCENT trim-graph -v -C 2 -G goss/${TEST}-merge -O goss/${TEST}-merge-trim
$TRANSLUCENT prune-tips -v --relative-cutoff 0.1 -G goss/${TEST}-merge-trim -O goss/${TEST}-merge-prune
$TRANSLUCENT pop-bubbles -v --relative-cutoff 0.1 -G goss/${TEST}-merge-prune -O goss/${TEST}-merge-clean
$TRANSLUCENT assemble -v -G goss/${TEST}-merge-clean -o goss/${TEST}-merge-assemble.fa -T ${THREADS} -i ${TEST}_1.${EXT} -i ${TEST}_2.${EXT}

