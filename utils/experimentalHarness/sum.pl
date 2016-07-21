#!/usr/bin/perl -w

my $sum = 0;

while (<>)
{
    next  if /Length/;
    chomp;
    my ($n,$name) = split(/\t/);
    next  if $n < 100;
    $sum += $n;
}

print "$sum\n";

