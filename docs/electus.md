% electus(1) Electus User Manual
% Bryan Beresford-Smith, Andrew Bromage, Thomas Conway, Jeremy Wazny
% June 22, 2012

NAME
====
electus - a tool for filtering reads.

Version 1.0.0

SYNOPSIS
========

electus index -T 8 --prefix idx --ref-fasta ref1.fa 

electus classify -T 8 --ref-prefix idx --ref-fasta ref2.fa --ref-threshold 1 --match-prefix match --non-match-prefix non --pairs -i in_1.fastq -i in_2.fastq

electus help

DESCRIPTION
===========

Second Generation Sequencing enables the collection of large quantities of 
sequence data. While such data sets are incredibly rich, often researchers 
are interested in answering specific questions about the data. Typically, 
a researcher with 100s of gigabytes of RNA-Seq data may be interested in 
SNPs and relative expression for only a small number of specific genes. In 
such a scenario, it is unnecessarily time consuming to process the entire 
data set against a whole reference set of genes, only to discard most of 
the analysis products.

Electus is a tool for quickly and sensitively extracting a relevant selection 
of reads.

Electus works by constructing the set of k-mers belonging to a set of references
and then classifying reads according to whether they contain k-mers in common 
with sufficiently many references. Often, only a single reference is required.
Reads which share enough k-mers with references are considered 'matching',
while all others are 'non-matching'.
The individual reads are partitioned into separate files according to this
classification. The ordering of reads in pairs is maintained.

<!--

Xenome has two distinct stages, which are embodied in two separate
commands: 'index' and 'classify'.
Before reads can be classified, an index must be constructed from 
the graft and host reference sequences. The references must be in FASTA 
format, and may optionally be compressed (gzip).

    xenome index -M 24 -T 8 -P idx -H mouse.fa -G human.fa

A xenome index consists of a number of related files which can be identified
by a user-specified prefix, e.g. 'idx' in the above command. The prefix may
contain '/' characters, allowing the index to be in a sub-directory. (Any 
such sub-directory must already exist - xenome will not create it.) 
For example, the set of files comprising an index with prefix 'idx' are:

~~~~~~~~~
idx-both.header
idx-both.kmers-d0
idx-both.kmers-d1
idx-both.kmers.header
idx-both.kmers.high-bits
idx-both.kmers.low-bits.lwr
idx-both.kmers.low-bits.upr
idx-both.lhs-bits
idx-both.rhs-bits
~~~~~~~~~

Once an index is available, reads can be classified according to whether they
appear to contain graft or host material. In the simplest case, Xenome can 
classify each read from a single source file individually.

    xenome classify -P idx -i in.fastq 

This step produces a file for each read category, containing all of the reads
which have been assigned that classification:

~~~~~~~~~
ambiguous.fastq
both.fastq
graft.fastq
host.fastq
neither.fastq
~~~~~~~~~

Input files are base-space reads in FASTA or FASTQ format or in a format
with one read per line and in either plain text or compressed format (gzip).

The files produced are in the same format as the input file, with all of the
input read data preserved. i.e. if the input reads are in FASTQ format, 
the reads written to each of the output files will also be in FASTQ format.

Multiple input files may be specified, but all inputs in the same format will
be written to the same set of output files.

    xenome classify -P idx -i inA.fastq -i inB.fastq -I inC.fasta

The above will result in the following set of files:

~~~~~~~~~
ambiguous.fasta
ambiguous.fastq
both.fasta
both.fastq
graft.fasta
graft.fastq
host.fasta
host.fastq
neither.fasta
neither.fastq
~~~~~~~~~

Each of the FASTQ files contains a mixture of reads from inA.fastq 
and inB.fastq. The FASTA files contain reads from inC.fasta.

If the combining of input reads from separate files is not desired, xenome
should be run separately for each input. The output from different runs 
can be distinguished by prefixing the filenames with a distinct string.

    xenome classify -P idx -i inA.fastq --output-filename-prefix A
    xenome classify -P idx -i inB.fastq --output-filename-prefix B

Running these two commands yields:

~~~~~~~~~
A_ambiguous.fastq
A_both.fastq
A_graft.fastq
A_host.fastq
A_neither.fastq
B_ambiguous.fastq
B_both.fastq
B_graft.fastq
B_host.fastq
B_neither.fastq
~~~~~~~~~

Xenome can also process pairs of reads.

    xenome classify -P idx --pairs -i in_1.fastq -i in_2.fastq

This results in a pair of files for each read category. The two reads of
each pair are written to the corresponding '_1' and '_2' files respectively.

~~~~~~~~~
ambiguous_1.fastq
ambiguous_2.fastq
both_1.fastq
both_2.fastq
graft_1.fastq
graft_2.fastq
host_1.fastq
host_2.fastq
neither_1.fastq
neither_2.fastq
~~~~~~~~~

If desired, more specific names can be used in place of 'host' and 'graft'.

    xenome classify -P idx -i in.fastq --graft-name human --host-name mouse

This will cause xenome to produce the following files.

~~~~~~~~~
ambiguous.fastq
both.fastq
human.fastq
mouse.fastq
neither.fastq
~~~~~~~~~

In addition to generating sets of output files, the classify command 
produces statistics about the number and proportion of reads assigned
to each category. These are printed to standard out at the end of a run
and look as follows:

    Statistics
    B       G       H       M       count     percent   class
    0       0       0       0       1900      0.938267  "neither"
    0       0       0       1       21        0.0103703 "both"
    0       0       1       0       28491     14.0696   "definitely host"
    0       0       1       1       7366      3.63751   "probably host"
    0       1       0       0       91895     45.38     "definitely graft"
    0       1       0       1       30059     14.8439   "probably graft"
    0       1       1       0       282       0.139259  "ambiguous"
    0       1       1       1       330       0.162962  "ambiguous"
    1       0       0       0       2878      1.42123   "both"
    1       0       0       1       254       0.125431  "probably both"
    1       0       1       0       610       0.301233  "definitely host"
    1       0       1       1       5815      2.87159   "probably host"
    1       1       0       0       3843      1.89777   "definitely graft"
    1       1       0       1       27775     13.716    "probably graft"
    1       1       1       0       99        0.0488886 "ambiguous"
    1       1       1       1       883       0.436047  "ambiguous"
    
    Summary
    count     percent   class
    153572    75.8377   "graft"
    42282     20.8799   "host"
    3153      1.55703   "both"
    1900      0.938267  "neither"
    1594      0.787157  "ambiguous"

Both tables contain a single heading line, followed by rows of TAB-separated 
elements; a format suitable for loading into R or a spreadsheet.

Each row represents the number and proportion of reads assigned to a 
particular class.
The B, G, H, and M fields represent the presence (1) or absence (0) of k-mers
belonging to the both, graft, host and marginal k-mer subsets, according to 
the reference index. 

The Statistics table contains 16 rows; one for each possible combination of
k-mer classes present within a read. The first row of the above table, 
indicates that for the given input, 1,900 reads (or pairs) - 0.938267% of
the total reads - contained no k-mers that belonged to the B, G, H, or M 
k-mer subsets, and are accordingly neither host nor graft reads.
Similarly, the fourteenth line states that 27,775 reads (or pairs) 
- 13.716% of the total - contained k-mers that belong to the B, G, M, but 
not H subsets, and are therefore "probably graft" reads.

In the Summary table, the B, G, H, and M columns are removed, and the classes
from the Statistics table have been collapsed into the five shown; the 
definitely/probably graft/host classes are combined into just graft/host
classes.
Notice that the different read output files, described earlier, correspond 
exactly to these classes.

-->

# OPTIONS COMMON TO ALL COMMANDS

The following options can be used with all of the *electus* commands and are therefore
not listed separately for each command.

-h,  \--help
:   Show a help message.

-l *FILE*, \--log-file *FILE*
:   Place to write progress messages. Messages are only written if the -v flag is used. 
    If omitted, messages are written to stderr.

-T *INT*, \--num-threads *INT*
:   The maximum number of *worker* threads to use. The actual number of threads
    used during the algorithms depends on each implementation. *electus* may use a small number
    of additional threads for performing non cpu-bound operations, such as file I/O.

\--tmp-dir *DIRECTORY*
:    A directory to use for temporary files.
     This flag may be repeated in order to nominate multiple temporary directories.

-v, \--verbose
:    Show progress messages.

-V, \--version
:    Show the software version.

# COMMANDS AND OPTIONS

## electus index

electus index [-k *INT*] [-M *INT*] -P *PREFIX* --ref-fasta *FASTA-filename*

Build an electus reference index from a single FASTA file.
The file may be gzip compressed, in which case the filename suffix must be *.gz*.

The k-mer size may be specified using the *-k* flag. If omitted, electus
defaults to k=25.

During index construction, electus maintains a hash table of the k-mers seen so
far. When this table fills, its contents are written to disk, and the table is
reinitialised. The more memory electus can use, the less often it will need to
write to disk, and the faster index construction will run. By default, 
electus will limit itself to 2 GB during index construction. 

The -M, --max-memory
flag can be used to explicitly control the amount of memory available to
electus (in GB). To improve performance, this should generally be set close to 
the amount memory available in the system - having accounted for operating 
system and other overhead.


*OPTIONS*

-k *INT*, \--kmer-size *INT*
:    The k-mer size to use for building the reference index: in version 1.0.0 
     this *must be an integer strictly less than 63*. If not supplied, the
     default value of 25 is used.

-M *INT*, \--max-memory *INT*
:    The maximum amount of memory (in GB) of memory to use.
     Making more memory available will reduce the number of times electus
     writes intermediate index data to disk.
     The default is 2 GB.

-P *PREFIX*, \--prefix *PREFIX*
:    The path prefix for all generated reference index files.
     The prefix may contain directory separators (e.g. '/') in order
     to have the index files written to another directory.

--ref-fasta *FILE*
:    The name of the FASTA file containing the host reference sequence.
     If the filename ends in *.gz* it will be read as a gzip file.

## xenome classify

xenome classify [--ref-index *PREFIX*]* [--ref-fasta *FASTA-filename*]* {-I *FASTA-filename* |  -i *FASTQ-filename* | --line-in *filename*}+ [--pairs] [--match-prefix *STRING*] [--nonmatch-prefix *STRING*] [--ref-threshold *INT*] [-k *INT*] [--single-sequence-refs]

Classifies input reads according to the k-mers they share with the given
references. References may be specified as either pre-built indices 
(see the 'index' command), or as sequences within FASTA files. By default,
all of the sequences within a single FASTA file are considered part of
the same reference, but if --single-sequence-refs is passed, each individual
sequence is treated as a separate reference.
Electus 1.0.0 supports up to 64 references.

All input reads (or pairs) are classified as either 'matching' or 'non-matching' 
and may be written to corresponding files. The names of these files are specified
by supplying prefixes via --match-prefix and --nonmatch-prefix. If a prefix is omitted,
the corresponding reads are not written to any file. e.g. if --nonmatch-prefix is
not given, non-matching reads are simply ignored.
By default a read (or pair) is considered 'matching' if it contains a k-mer in
common with each of the references. This requirement may be relaxed with the
--ref-threshold flag, which can be used to set the minimum number of 
references that the read must share k-mers with in order to be considered 
'matching'. e.g. --ref-threshold 1 implies that a read only needs a single
k-mer in common with any reference to be 'matching'.


*OPTIONS*

--ref-index *PREFIX*
:    The path prefix for a single reference's index files.
     The prefix may contain directory separators (e.g. '/') in order
     to have the index files written to another directory.
     Multiple reference indices may be specified and there is no
     requirement that they were built using the same k-mer size.

--ref-fasta *FASTA-filename*
:    The path to a reference FASTA file.

-i *FILE*, \--fastq-in *FILE*
:    Input file in FASTQ format.

-I *FILE*, \--fasta-in *FILE*
:    Input file in FASTA format.

--line-in *FILE*
:    Input file with one read per line and no other annotation.

--pairs
:    Treat reads from consecutive input files of the same type as pairs.

--match-prefix *STRING*
:    The filename prefix for matching reads. The actual filename(s)
     will consist of a prefix, then '_1' or '_2' if the file contains
     reads from a pair, and finally a suffix corresponding to the read
     format, i.e. '.fastq', '.fasta', or '.txt'.
     Note that the prefix may contain path separators, e.g. 'out/match',
     in order to deposit the files in another directory.
     If this flag is omitted, matching reads are not written to any file.

--non-match-prefix *STRING*
:    The filename prefix for non-matching reads. The actual filename(s)
     are constructed in the same way as for the matching reads.
     If this flag is omitted, non-matching reads are not written to any file.

-k *INT*, \--kmer-size *INT*
:    The k-mer size to use for building k-mer sets from reference FASTA files: 
     in version 1.0.0 this *must be an integer strictly less than 63*. If not 
     supplied, the default value of 25 is used.
     Note that this value of k applies only to the k-mers extracted from 
     FASTA files passed in via --ref-fasta. It need not be the same value as
     the k used for any prebuilt reference index, as specified by --ref-index.

--ref-threshold *INT*
:    Specifies the minimum number of references that a read must share k-mers
     with in order to be considered 'matching'.
     If this flag is not specified, the default is all of the references.

--single-sequence-refs
:    Treat each sequence in each reference FASTA file as a separate reference.
     By default all of the sequences in each file are considered to belong
     to the same reference.

## electus help

electus help

Prints a summary of all of the electus commands.


EXAMPLES
========

Eliminating bacterial contamination by extracting all possibly-human reads.

Accelerating gene expression level analysis by extracting gene-specific reads. (Just the exons? Lots of genes?)

Speeding up the search for gene fusions by extracting reads that contain k-mers belonging to more than one reference. 

--

# LIMITATIONS


# FUTURE RELEASES

Bzip support will be introduced. 

