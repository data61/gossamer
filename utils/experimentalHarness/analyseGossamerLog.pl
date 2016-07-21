#!/usr/bin/perl -w

my @passes = qw(build-graph trim prune-1 prune-2 prune-3 prune-4 pop-bubbles entry-edge-sets read-sets supergraph thread-pairs);
my @lognames = qw(graph.log graphC3.log graphC3-p1.log graphC3-p2.log graphC3-p3.log graphC3-p4.log  graphC3-p4-popb.log build-entry-edge-set.log build-read-sets.log build-supergraph.log thread-pairs.log);
my %timings = ();
my %memusages = ();

sub hms($)
{
    my ($datestr) = @_;
    my ($h,$m,$s) = $datestr =~ /([0-9]*):([0-9]*):([0-9]*)/;
    return $h * 3600 + $m * 60 + $s;
}

while (<>)
{
}


