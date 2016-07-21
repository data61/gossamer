#!/usr/bin/perl -w

my $genome_size = undef;
if (defined($ARGV[0]) && $ARGV[0] eq "--ng50")
{
    shift @ARGV;
    $genome_size = $ARGV[0];
    shift @ARGV;
}

my @data = ();
my $sum = 0;

while (<>)
{
    next  if /Length/;
    chomp;
    my ($n,$name) = split(/\t/);
    next  if $n < 100;
    $sum += $n;
    push(@data, $n);
}

@data = sort { $b <=> $a } (@data);

my $a50 = (defined($genome_size) ? $genome_size : $sum) / 2;

my $partialsum = 0;
foreach my $v (@data)
{
    $partialsum += $v;
    if ($partialsum >= $a50)
    {
	print "$v\n";
	last;
    }
}

