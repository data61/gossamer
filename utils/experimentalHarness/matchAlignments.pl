#!/usr/bin/perl -w

use strict;
use feature "switch";

my $thresh = 0.99;

while (<>)
{
    next  if /^@(?:HD|SQ|RG|PG|CO)/;
    chomp;
    my @line = split(/\t/, $_);
    my $name = $line[0];
    my $cigar = $line[5];

    my @entries = split(/(?<=[MDINSHP=X])/, $cigar);
    my $len = 0;
    my $match = 0;
    foreach my $entry(@entries)
    {
        my ($n,$c) = $entry =~ /^(\d*)([MDINSHP=X])$/;
        $n = 1  if $n eq '';
        given ($c)
        {
            when (/[MIS=XD]/)
            {
                $len += $n;
		$match += $n  if $c eq 'M';
            }
            default
            {
		die "Unknown control code $c in string $cigar";
            }
        }
    }
    if ($match / $len >= $thresh)
    {
	print $name, "\n";
    }
}


