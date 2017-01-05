#!/bin/bash

out='contigs.fa'
trim='auto'
wd='goss-files'
B='2'
K='27'
V=''
T='-T 4'
ech=false
doit=true
td=''

function usage()
{
    echo "usage: $0 [options and files]...." 1>&2
    echo "  -B <buffer-size>        amount of buffer space to use in GB (default 2)." 1>&2
    echo "  -c <coverage>           coverage estimate to use during pair/read threading." 1>&2
    echo "                          (defaults to using automatic coverage estimation)" 1>&2
    echo "  -D                      don't execute the goss commands." 1>&2
    echo "  -h                      print this help message (add -v for a longer version)." 1>&2
    echo "  -k <kmer-size>          kmer size for the de Bruijn graph (default 27, max 62)." 1>&2
    echo "  -m <insert-size>        introduce files containing mate pairs with the given insert size." 1>&2
    echo "                          files following (until the next -m or -p) will be treated as mate pairs." 1>&2
    echo "  -o <file-name>          file-name for the contigs. (default 'contigs.fa')" 1>&2
    echo "  -p <insert-size>        introduce files containing paired end reads with the given insert size." 1>&2
    echo "                          files following (until the next -m or -p) will be treated as paired ends." 1>&2
    echo "  -T <num-threads>        use the given number of worker threads (default 4)." 1>&2
    echo "  -t <coverage>           trim edges with coverage less than the given value." 1>&2
    echo "                          (defaults to using automatic trim value estimation)" 1>&2
    echo "  -V                      print verbose output from the individual goss commands." 1>&2
    echo "  -v                      print the goss commands." 1>&2
    echo "  -w <dir>                directory for graphs and other working files." 1>&2
    echo "                          (default 'goss-files')" 1>&2
    echo "  --tmp-dir <dir>         directory for temporary files. (default '/tmp')"
    if $ech
    then
        echo "" 1>&2
        echo "Examples:" 1>&2
        echo "" 1>&2
        echo "Perform the simplest possible assembly on unpaired reads" 1>&2
        echo "and write the contigs to 'output.fa':" 1>&2
        echo "    $0 -o output.fa reads1.fastq.gz reads2.fastq.gz" 1>&2
        echo "" 1>&2
        echo "Perform a simple assembly on unpaired reads assuming a" 1>&2
        echo "machine with 8 cores and 24GB of useable RAM:" 1>&2
        echo "    $0 -T 8 -B 24 -o output.fa reads1.fastq.gz reads2.fastq.gz" 1>&2
        echo "" 1>&2
        echo "Assemble reads from a paired end library with an insert size" 1>&2
        echo "of 500bp, and a mate pair library with an insert size of 2kb" 1>&2
        echo "using k=55, 8 cores and 24GB of RAM:" 1>&2
        echo "    $0 -T 8 -B 24 -k 55 -o output.fa \\" 1>&2
        echo "            -p 500 paired_1.fastq paired_2.fastq \\" 1>&2
        echo "            -m 2000 mate_1.fastq mate_2.fastq \\" 1>&2
        echo "" 1>&2
        echo "Have $0 print out the indivitual 'goss' commands for a simple" 1>&2
        echo "assembly, without actually executing them:" 1>&2
        echo "    $0 -D -v -o output.fa reads1.fastq.gz reads2.fastq.gz" 1>&2
    fi
    exit 0
}

curGrpNum=0
dohelp=true
while [ "$1" != "" ]
do
    case $1 in
        -B) shift; dohelp=false; B="$1"; shift;;
        -c) shift; dohelp=false; cov="--expected-coverage $1"; shift;;
        -D) shift; dohelp=false; doit=false;;
        -h) shift; dohelp=true;;
        -k) shift; dohelp=false; K="$1"; shift;;
        -m) shift; dohelp=false; 
            curGrpNum=$(($curGrpNum+1));
            ins[$curGrpNum]="$1";
            kinds[$curGrpNum]='mp';
            shift;;
        -o) shift; dohelp=false; out="$1"; shift;;
        -p) shift; dohelp=false;
            curGrpNum=$(($curGrpNum+1));
            ins[$curGrpNum]="$1";
            kinds[$curGrpNum]='pe';
            shift;;
        -T) shift; dohelp=false; T="-T $1"; shift;;
        -t) shift; dohelp=false; trim="$1"; shift;;
        --tmp-dir) shift; dohelp=false; td="--tmp-dir $1"; shift;;
        -v) shift; ech=true;;
        -V) shift; dohelp=false; V="-v";;
        -w) shift; dohelp=false; wd="$1"; shift;;
        *.fa|*.fasta|*.fa.gz|*.fa.bz2|*.fasta.gz|*.fasta.bz2) 
            grps[$curGrpNum]="${grps[$curGrpNum]} -I $1"; shift; dohelp=false;;
        *.fq|*.fastq|*.fq.gz|*.fq.bz2|*.fastq.gz|*.fastq.bz2) 
            grps[$curGrpNum]="${grps[$curGrpNum]} -i $1"; shift; dohelp=false;;
        *s_?_sequence.txt|*s_?_sequence.txt.gz|*s_?_sequence.txt.bz2) 
            grps[$curGrpNum]="${grps[$curGrpNum]} -i $1"; shift; dohelp=false;;
        *s_?_?_sequence.txt|*s_?_?_sequence.txt.gz|*s_?_?_sequence.txt.bz2) 
            grps[$curGrpNum]="${grps[$curGrpNum]} -I $1"; shift; dohelp=false;;
        *) echo "unrecognised option/filename: $1"; dohelp=true; shift;;
    esac
done

if $dohelp
then
    usage
fi

goss=${GOSSAMER:-goss}

if $doit && ! which $goss > /dev/null
then
    echo "cannot find 'goss' executable"
    exit 1
fi

$ech && $goss help --version

if [ $K -ge 63 ]
then
    echo "maximum k is 62"
    exit 1
fi

mkdir "$wd" 2>/dev/null

files="${grps[@]}"

gn=1
g="$wd/graph-$gn"
$ech  && echo $goss build-graph $V $T $td -k "$K" -B "$B" -O "$g" $files
$doit && $goss build-graph $V $T $td -k "$K" -B "$B" -O "$g" $files || doit=false
gn=$((gn+1))
h="$wd/graph-$gn"
if [ "$trim" = "auto" ]
then
    $ech  && echo $goss trim-graph $V $T $td -G "$g" -O $h
    $doit && $goss trim-graph $V $T $td -G "$g" -O $h || doit=false
else
    $ech  && echo $goss trim-graph $V $T $td -G "$g" -O $h -C $(($trim+1))
    $doit && $goss trim-graph $V $T $td -G "$g" -O $h -C $(($trim+1)) || doit=false
fi
for i in 1 2 3 4 5
do
    g="$h"
    gn=$((gn+1))
    h="$wd/graph-$gn"
    $ech && echo $goss prune-tips $V $T $td -G "$g" -O "$h"
    $doit && $goss prune-tips $V $T $td -G "$g" -O "$h" || doit=false
done
g="$h"
gn=$((gn+1))
h="$wd/graph-$gn"
$ech && echo $goss pop-bubbles $V $T $td -G "$g" -O "$h"
$doit && $goss pop-bubbles $V $T $td -G "$g" -O "$h" || doit=false
g="$h"
$ech && echo $goss build-entry-edge-set $V $T $td -G "$g"
$doit && $goss build-entry-edge-set $V $T $td -G "$g" || doit=false
$ech && echo $goss build-supergraph $V $T $td -G "$g"
$doit && $goss build-supergraph $V $T $td -G "$g" || doit=false

i=1;
while [ $i -le $curGrpNum ]
do
    fs="${grps[$i]}"
    p="${ins[$i]}"
    if [ "${kinds[$i]}" = "pe" ]
    then
        fm="--paired-ends"
    else
        fm="--mate-pairs"
    fi
    $ech  && echo $goss thread-pairs $V $T $td -G "$g" --insert-expected-size "$p" $cov $fm $fs
    $doit && $goss thread-pairs $V $T $td -G "$g" --search-radius 10 --insert-expected-size "$p" $cov $fm $fs || doit=false
    i=$(($i+1))
done

$ech  && echo $goss thread-reads $V $T $td -G "$g" $cov $files
$doit && $goss thread-reads $V $T $td -G "$g" $cov $files || doit=false

i=1;
while [ $i -le $curGrpNum ]
do
    fs="${grps[$i]}"
    p="${ins[$i]}"
    if [ "${kinds[$i]}" = "pe" ]
    then
        fm="--paired-ends"
    else
        fm="--mate-pairs"
    fi
    $ech  && echo $goss build-scaffold $V $T $td -G "$g" --insert-expected-size "$p" $cov $fm $fs
    $doit && $goss build-scaffold $V $T $td -G "$g" --insert-expected-size "$p" $cov $fm $fs || doit=false
    i=$(($i+1))
done

if [ $curGrpNum -gt 0 ]
then
    $ech  && echo $goss scaffold $V $T $td -G "$g"
    $doit && $goss scaffold $V $T $td -G "$g" || doit=false
fi

$ech  && echo $goss print-contigs $V $T $td -G "$g" --min-length 100 -o "$out"
$doit && $goss print-contigs $V $T $td -G "$g" --min-length 100 -o "$out" || doit=false
