#!/usr/bin/perl -w
use strict;


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
            if ($length >= 100)
            {
                print "$name\n$contig\n";
            }
            $length = 0;
            $contig = '';
        }
        $name = $line;
    	next;
    }
    $length += length($line);
    $contig .= $line;
}
print "$name\n$contig\n"  if !$first && $length >= 100;

1;
