#!/bin/bash

out='contigs.fa'
trim='1'
t1='0.1'
t2=''
wd='trans-files'
B='2'
K=()
V=''
T='-T 4'
ech=false
doit=true
td=''

usage()
{
    echo "usage: $0 [options and files]...." 1>&2
    echo "  -B <buffer-size>        amount of buffer space to use in GB (default 2)." 1>&2
    echo "  -D                      don't execute the translucent commands." 1>&2
    echo "  -h                      print this help message (add -v for a longer version)." 1>&2
    echo "  -k <kmer-size>          add a kmer size for the de Bruijn graph." 1>&2
    echo "                          (defaults 19,23,27,31,35; max 62)" 1>&2
    echo "  -o <file-name>          file-name for the contigs. (default 'contigs.fa')" 1>&2
    echo "  -T <num-threads>        use the given number of worker threads (default 4)." 1>&2
    echo "  -t <coverage>           trim edges with coverage less than the given value." 1>&2
    echo "                          (defaults to 1)" 1>&2
    echo "  -t1 <rel-coverage>      prune paths with relative coverage less than the " 1>&2
    echo "                          given value for the initial pass. (defaults to 0.1)" 1>&2
    echo "  -t2 <rel-coverage>      prune paths with relative coverage less than the " 1>&2
    echo "                          given value for the final pass. (defaults to nothing)" 1>&2
    echo "  -V                      print verbose output from the individual translucent commands." 1>&2
    echo "  -v                      print the translucent commands." 1>&2
    echo "  -w <dir>                directory for graphs and other working files." 1>&2
    echo "                          (default 'trans-files')" 1>&2
    echo "  --tmp-dir <dir>         directory for temporary files. (default '/tmp')"
#    if $ech
#    then
#        echo "" 1>&2
#        echo "Examples:" 1>&2
#        echo "" 1>&2
#        echo "Perform the simplest possible assembly on unpaired reads" 1>&2
#        echo "and write the contigs to 'output.fa':" 1>&2
#        echo "    $0 -o output.fa reads1.fastq.gz reads2.fastq.gz" 1>&2
#        echo "" 1>&2
#        echo "Perform a simple assembly on unpaired reads assuming a" 1>&2
#        echo "machine with 8 cores and 24GB of useable RAM:" 1>&2
#        echo "    $0 -T 8 -B 24 -o output.fa reads1.fastq.gz reads2.fastq.gz" 1>&2
#        echo "" 1>&2
#        echo "Assemble reads from a paired end library with an insert size" 1>&2
#        echo "of 500bp, and a mate pair library with an insert size of 2kb" 1>&2
#        echo "using k=55, 8 cores and 24GB of RAM:" 1>&2
#        echo "    $0 -T 8 -B 24 -k 55 -o output.fa \\" 1>&2
#        echo "            -p 500 paired_1.fastq paired_2.fastq \\" 1>&2
#        echo "            -m 2000 mate_1.fastq mate_2.fastq \\" 1>&2
#        echo "" 1>&2
#        echo "Have $0 print out the indivitual 'goss' commands for a simple" 1>&2
#        echo "assembly, without actually executing them:" 1>&2
#        echo "    $0 -D -v -o output.fa reads1.fastq.gz reads2.fastq.gz" 1>&2
#    fi
    exit 0
}

curGrpNum=0
dohelp=true
while [ "x$1" != "x" ]
do
    case $1 in
        -B) shift; dohelp=false; B="$1"; shift;;
        -D) shift; dohelp=false; doit=false;;
        -h) shift; dohelp=true;;
        -k) shift; dohelp=false; K+=("$1"); shift;;
        -o) shift; dohelp=false; out="$1"; shift;;
        -T) shift; dohelp=false; T="-T $1"; shift;;
        -t) shift; dohelp=false; trim="$1"; shift;;
        -t1) shift; dohelp=false; t1="$1"; shift;;
        -t2) shift; dohelp=false; t2="$1"; shift;;
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

trans=${TRANSLUCENT:-translucent}

if $doit && ! which $trans > /dev/null
then
    echo "cannot find 'translucent' executable"
    exit 1
fi

$ech && $trans help --version

# Sanity check $K.
if [ ${#K} -lt 1 ]
then
    K+=(19 23 27 31 35)
fi
MAXK=0
for k in ${K[@]}
do
    if [ $k -ge 63 ]
    then
        echo "maximum k is 62"
        exit 1
    fi
    if [ $k -le $MAXK ]
    then
        echo "kmer sizes must be specified in ascending order"
        exit 1
    fi
    MAXK="$k"
done

mkdir "$wd" 2>/dev/null

files="${grps[@]}"

for k in ${K[@]}
do
    gn=1
    h="$wd/graph-$k-$gn"
    $ech  && echo $trans build-graph $V $T $td -k "$K" -B "$B" -O "$h" $files
    $doit && $trans build-graph $V $T $td -k "$K" -B "$B" -O "$h" $files || doit=false

    if [ "x$trim" != "x" ]
    then
        g="$h"
        gn=$((gn+1))
        h="$wd/graph-$k-$gn"
        $ech  && echo $trans trim-graph $V $T $td -G "$g" -O $h -C $(($trim+1))
        $doit && $trans trim-graph $V $T $td -G "$g" -O $h -C $(($trim+1)) || doit=false
    fi

    if [ "x$t1" != "x" ]
    then
        g="$h"
        gn=$((gn+1))
        h="$wd/graph-$k-$gn"
        $ech && echo $trans trim-relative $V $T $td -G "$g" -O "$h" --relative-cutoff "$t1"
        $doit && $trans trim-relative $V $T $td -G "$g" -O "$h" --relative-cutoff "$t1" || doit=false

        g="$h"
        gn=$((gn+1))
        h="$wd/graph-$k-$gn"
        $ech && echo $trans prune-tips $V $T $td -G "$g" -O "$h" --relative-cutoff "$t1"
        $doit && $trans prune-tips $V $T $td -G "$g" -O "$h" --relative-cutoff "$t1" || doit=false

        g="$h"
        gn=$((gn+1))
        h="$wd/graph-$k-$gn"
        $ech && echo $trans pop-bubbles $V $T $td -G "$g" -O "$h" --relative-cutoff "$t1"
        $doit && $trans pop-bubbles $V $T $td -G "$g" -O "$h" --relative-cutoff "$t1" || doit=false
    fi

    contigout="contig-$k.fa"

    contigs[$k]="$contigout"

    $ech  && echo $trans assemble $V $T $td -G "$g" --min-length 100 -o "$contigout" $files
    $doit && $trans assemble $V $T $td -G "$g" --min-length 100 -o "$contigout" $files || doit=false
done

if [ ${#K} -eq 1 ]
then
    # Only one k-mer size, so we're done.
    $ech  && echo mv "${contigs[@]}" $out
    $doit && mv "${contigs[@]}" $out || doit=false
    exit 0
fi

allcontigs="${contigs[@]}"

gn=1
h="$wd/graph-merge-$gn"
$ech  && echo $trans build-graph $V $T $td -k "$K" -B "$B" -O "$h" $allcontigs
$doit && $trans build-graph $V $T $td -k "$K" -B "$B" -O "$h" $files || doit=false

g="$h"
gn=$((gn+1))
h="$wd/graph-merge-$gn"
ref="$wd/graph-$MAXK-1"
$ech && echo $trans merge-graph-with-reference $V $T $td -G "$g" -O "$h" --graph-ref "$ref"
$doit && $trans merge-graph-with-reference $V $T $td -G "$g" -O "$h" --graph-ref "$ref" || doit=false

if [ "x$t2" != "x" ]
then
    g="$h"
    gn=$((gn+1))
    h="$wd/graph-merge-$gn"
    $ech && echo $trans trim-relative $V $T $td -G "$g" -O "$h" --relative-cutoff "$t2"
    $doit && $trans trim-relative $V $T $td -G "$g" -O "$h" --relative-cutoff "$t2" || doit=false

    g="$h"
    gn=$((gn+1))
    h="$wd/graph-merge-$gn"
    $ech && echo $trans prune-tips $V $T $td -G "$g" -O "$h" --relative-cutoff "$t2"
    $doit && $trans prune-tips $V $T $td -G "$g" -O "$h" --relative-cutoff "$t2" || doit=false

    g="$h"
    gn=$((gn+1))
    h="$wd/graph-merge-$gn"
    $ech && echo $trans pop-bubbles $V $T $td -G "$g" -O "$h" --relative-cutoff "$t2"
    $doit && $trans pop-bubbles $V $T $td -G "$g" -O "$h" --relative-cutoff "$t2" || doit=false
fi

g="$h"
$ech  && echo $trans assemble $V $T $td -G "$g" --min-length 100 -o "$out" $files
$doit && $trans assemble $V $T $td -G "$g" --min-length 100 -o "$out" $files || doit=false

exit 0
