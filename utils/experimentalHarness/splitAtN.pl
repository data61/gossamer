#!/usr/bin/perl -w
use strict;

sub outputContigs($$)
{
    my ($name,$contig) = @_;

    my @contigs = split(/N+/, $contig);
    if ($#contigs < 2)
    {
        print "$name\n$contig\n";
        return;
    }

    my $i = 0;
    foreach my $c (@contigs)
    {
        print $name, "___", $i, "\n";
        print $c, "\n";
    }
}


my $length = 0;
my $first = 1;
my %histo = ();
my $name = '';
my $contig = '';
while (<>)
{
    chomp;
    my $line = $_;
    my $temp = substr($line, length($line) - 1, 1);
    if ($temp eq "\r" || $temp eq "\n")
    {
        chop($line);
    }
    if ($line =~ /^>/)
    {
    	if ($first)
    	{
    	    $first = 0;
    	}
        else
        {
            outputContigs($name,$contig);
            $length = 0;
            $contig = '';
        }
        $name = $line;
    	next;
    }
    $length += length($line);
    $contig .= $line;
}
outputContigs($name,$contig)  if !$first;

1;
