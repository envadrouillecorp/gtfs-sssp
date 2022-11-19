#!/usr/bin/perl
use strict;
use warnings;
use v5.10;
use Data::Dumper;
use Encode;
use JSON qw( decode_json encode_json );     # From CPAN

# Merge outputs of the parser

my @files = @ARGV;
my %best;

for my $f (@files) {
   my $text = `cat "$f"`;
   $text =~ s/:/=>/smg; #Output of the parser is for JS, but easy to transform to perl code
   my $stops = eval($text);
   foreach my $stop (@$stops) {
      my $old = $best{$stop->{dst}};
      if(!$old || $stop->{dur} < $old->{dur}) {
         $best{$stop->{dst}} = $stop;
      }
   }
}

my @values = values %best;
print JSON->new->utf8(0)->encode(\@values);
