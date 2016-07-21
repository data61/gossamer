% Gossamer README
% June, 2011

Gossamer is a tool for *de novo* assembly of high throughput sequencing data.

REQUIREMENTS
===========

Gossamer should run on any standard 64 bit Linux environment.
Mammalian size genomes have been assembled using goss on a 32 GB Ubuntu machine.

INSTALLATION
===========

The Gossamer program and related files (including this README.txt) will be 
extracted to a directory called goss-*version* where *version* is the version
number for the distribution. For example, for version 0.1.1 this will be the
directory goss-0.1.1.

The following files should appear in this directory:

***

---------------------------------------------------------------------------
File Name               Description
-----------             -------------
goss                    The Gossamer program.

goss.pdf                The Gossamer manual in pdf format.

goss.1                  The Gossamer unix manpage. To view use :
                            man ./goss.1

gossple.pdf             A manual for the simple script for invoking Gossamer
                        in pdf format.

gossple.1               The gossple unix manpage. To view use :
                            man ./gossple.1

license.txt             The license for goss.

README.txt              This readme file.

example                 A directory containing some test data.

example/reference.fa    The genome from which the reads have been derived.

example/reads1.fq.gz    Reads in fastq format (first in pair).

example/reads2.fq.gz    Reads in fastq format (second in pair).

example/build.sh        A shell script which will assemble the reads and
                        print the contigs using the gossple script.

---------------------------------------------------------------------------

***

Please refer to the gossamer and gossple manuals for further details and 
for examples of using goss.

This software is distributed under the included license.
If you use the gossamer assembler, please cite the following paper:

Thomas C Conway, Andrew J Bromage, "Succinct data structures for assembling large genomes", 
Bioinformatics, 2011 vol. 27 (4) pp. 479-86

