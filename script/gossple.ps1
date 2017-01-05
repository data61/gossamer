$global:ech=$false
$out='contigs.fa'
$trim='auto'
$wd='goss-files'
$B='2'
$K='27'
$V=''
$T='-T 4'
$global:doit=$true
$td=''
$goss=''
$kill_signal=''

function usage
{
    echo "usage: $global:args[0] [options and files]...."
    echo "  -B <buffer-size>        amount of buffer space to use in GB (default 2)."
    echo "  -c <coverage>           coverage estimate to use during pair/read threading."
    echo "                          (defaults to using automatic coverage estimation)"
    echo "  -D                      don't execute the goss commands."
    echo "  -h                      print this help message (add -v for a longer version)."
    echo "  -k <kmer-size>          kmer size for the de Bruijn graph (default 27, max 62)."
    echo "  -m <insert-size>        introduce files containing mate pairs with the given insert size."
    echo "                          files following (until the next -m or -p) will be treated as mate pairs."
    echo "  -o <file-name>          file-name for the contigs. (default 'contigs.fa')"
    echo "  -p <insert-size>        introduce files containing paired end reads with the given insert size."
    echo "                          files following (until the next -m or -p) will be treated as paired ends."
    echo "  -T <num-threads>        use the given number of worker threads (default 4)."
    echo "  -t <coverage>           trim edges with coverage less than the given value."
    echo "                          (defaults to using automatic trim value estimation)"
    echo "  -V                      print verbose output from the individual goss commands."
    echo "  -v                      print the goss commands."
    echo "  -w <dir>                directory for graphs and other working files."
    echo "                          (default 'goss-files')"
    echo "  --tmp-dir <dir>         directory for temporary files. (default '/tmp')"
    if($ech) {
        echo ""
        echo "Examples:"
        echo ""
        echo "Perform the simplest possible assembly on unpaired reads"
        echo "and write the contigs to 'output.fa':"
        echo "    $0 -o output.fa reads1.fastq.gz reads2.fastq.gz"
        echo ""
        echo "Perform a simple assembly on unpaired reads assuming a"
        echo "machine with 8 cores and 24GB of useable RAM:"
        echo "    $0 -T 8 -B 24 -o output.fa reads1.fastq.gz reads2.fastq.gz"
        echo ""
        echo "Assemble reads from a paired end library with an insert size"
        echo "of 500bp, and a mate pair library with an insert size of 2kb"
        echo "using k=55, 8 cores and 24GB of REAM:"
        echo "    $0 -T 8 -B 24 -k 55 -o output.fa \\"
        echo "            -p 500 paired_1.fastq paired_2.fastq \\"
        echo "            -m 2000 mate_1.fastq mate_2.fastq \\"
        echo ""
        echo "Have $0 print out the indivitual 'goss' commands for a simple"
        echo "assembly, without actually executing them:"
        echo "    $0 -D -v -o output.fa reads1.fastq.gz reads2.fastq.gz"
    }
    exit 0
}

function Get-ScriptDirectory
{
    $Invocation = (Get-Variable MyInvocation -Scope 1).Value
    Split-Path $Invocation.MyCommand.Path
}

function shift
{
    $global:idx++
}

function arg
{
    $global:args[$global:idx]
    return

}

function run( $cmd )
{
    if( $global:ech ) {
        echo $cmd 
    }
    if( $global:doit ) {
        invoke-expression $cmd
        if( $LastExitCode -ne 0 ) {
            $global:doit = $false
        }
    }
}

# this affects line wrapping in the console output
$host.UI.RawUI.BufferSize = new-object System.Management.Automation.Host.Size(2048,50)

$global:args = $args

$fastaRegex = ".*\.fa$|.*\.fasta$|.*\.fa\.gz$|.*\.fa\.bz2$|.*\.fasta\.gz$|.*\.fasta\.bz2$ 
               |.*s_._._sequence\.txt$|.*s_._._sequence\.txt\.gz$|.*s_._._sequence\.txt\.bz2$"
               
$fastqRegex = ".*\.fq$|.*\.fastq$|.*\.fq\.gz$|.*\.fq\.bz2$|.*\.fastq\.gz$|.*\.fastq\.bz2$
               |.*s_._sequence\.txt$|.*s_._sequence\.txt\.gz$|.*s_._sequence\.txt\.bz2$"


$dohelp=$false
$ins=@('')
$kinds=@('')
$grps=@('')
$global:idx = 1; # skip name of this script
while( $global:idx -lt $global:args.Length ) {
    echo "arg: $(arg)"
    switch -casesensitive ( $(arg) ) 
    { 
        "-x" { shift; $goss=$(arg); shift; }
        "-B" { shift; $B=$(arg); shift; }
        "-c" { shift; $cov="--expected-coverage $(arg)"; shift; }
        "-D" { shift; $global:doit=$false; }
        "-h" { shift; $dohelp=$true; }
        "-k" { shift; $K=$(arg); shift; }
        "-m" { shift;
               $ins=$ins+$(arg);
               $kinds=$kinds+'mp';
               $grps=$grps+'';
               shift; }
        "-o" { shift; $out=$(arg); shift; }
        "-p" { shift;
               $ins=$ins+$(arg);
               $kinds=$kinds+'pe';
               $grps=$grps+'';
               shift; }
        "-T" { shift; $T="-T $(arg)"; shift; }
        "-t" { shift; $trim=$(arg); shift; }
        "--tmp-dir" { shift; $td="--tmp-dir $(arg)"; shift; }
        "-v" { shift; $global:ech=$true; }
        "-V" { shift; $V="-v"; }
        "-w" { shift; $wd=$(arg); shift; }
        "--kill-signal" { shift; $kill_signal="--kill-signal $(arg)"; shift; }
        default {
            switch -regex ( $(arg) ) {
                $fastaRegex { $grps[$grps.Length-1] = $grps[$grps.Length-1] + " -I $(arg)"; shift; }
                $fastqRegex { $grps[$grps.Length-1] = $grps[$grps.Length-1] + " -i $(arg)"; shift; }
                default { echo "unrecognised option/filename: $(arg)"; $dohelp=$true; shift }
            }
        }
    }    
}

if( $dohelp ) {
    usage
}

if( $goss -eq '' ) { # default goss executable
    $goss = $(Get-ScriptDirectory) + "/goss.exe"
}

if( ! $( Test-Path $goss ) ) {
    echo "cannot find '$goss'";
    exit 1
}

run "$goss help --version"

if( $K -ge 63 ) {
    echo "maximum k is 62"
    exit 1
}

if( ! $( Test-Path $wd ) ) {
    mkdir $wd > $null
}

$scaf="$wd/scaf"

$files = [string]::join('', $grps)

$gn=1
$g="$wd/graph-$gn"
run "$goss build-graph $kill_signal $V $T $td -k $K -B $B -O $g $files"
$gn=$gn+1
$h="$wd/graph-$gn"
if( "$trim" -eq "auto" ) {
    run "$goss trim-graph $kill_signal $V $T $td -G $g -O $h"
}
else {
    run "$goss trim-graph $kill_signal $V $T $td -G $g -O $h -C $($trim+1)"
}

foreach( $i in 1..5 ) {
    $g=$h
    $gn=$gn+1
    $h="$wd/graph-$gn"
    run "$goss prune-tips $kill_signal $V $T $td -G $g -O $h"
}

$g="$h"
$gn=$gn+1
$h="$wd/graph-$gn"
run "$goss pop-bubbles $kill_signal $V $T $td -G $g -O $h"

$g="$h"
run "$goss build-entry-edge-set $kill_signal $V $T $td -G $g"
run "$goss build-supergraph $kill_signal $V $T $td -G $g"

for( $i=1; $i -lt $grps.Length; $i++ ) {
    $fs=$grps[$i]
    $p=$ins[$i]
    if( $kinds[$i] -eq "pe" ) {
        $fm="--paired-ends"
    }
    else {
        $fm="--mate-pairs"
    }
    run "$goss thread-pairs $V $T $td -G $g --search-radius 10 --insert-expected-size $p $cov $fm $fs"
}

run "$goss thread-reads $kill_signal $V $T $td -G $g $cov $files"

for( $i=1; $i -lt $grps.Length; $i++ ) {
    $fs=$grps[$i]
    $p=$ins[$i]
    if( $kinds[$i] = "pe" ) {
        $fm="--paired-ends"
    }
    else {
        $fm="--mate-pairs"
    }
    run "$goss build-scaffold $kill_signal $V $T $td -G $g --insert-expected-size $p $cov $fm $fs --scaffold-out $scaf"
}

if( $grps.Length -gt 0 ) {
    run "$goss scaffold $kill_signal $V $T $td -G $g --scaffold-in $scaf"
}

run "$goss print-contigs $kill_signal $V $T $td -G $g --min-length 100 -o $out"
