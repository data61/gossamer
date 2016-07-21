#!/usr/bin/perl -w
use strict;


my $length = 0;
my $first = 1;
my %histo = ();
my $name = '';
print "Length\tName\n";
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
            print "$length\t$name\n";
            $length = 0;
        }
        $line =~ s/^>//;
        $name = $line;
    	next;
    }
    $length += length($line);
}
print "$length\t$name\n"  if !$first;

1;
