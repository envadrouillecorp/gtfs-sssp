#!/usr/bin/perl
use strict;
use warnings;
use LWP::Simple;                # From CPAN
use JSON qw( decode_json );     # From CPAN
use Data::Dumper;               # Perl core module

my $osmr = "http://localhost:5000/route/v1/transit/%f,%f;%f,%f?overview=full&geometries=geojson";

if($ARGV[1]) {
	die "./query-osrm.pl <json output of parser>\n";
}

# Open json output
open(FH,"<",$ARGV[1]) or die "$ARGV[1] doesn't exists!\n";
my $text = <FH>;
my $stops = decode_json( $text );

# Foreach stop
foreach my $stop (@$stops) { 
	#{ dst:"Nîmes", dstlat:43.8324, dstlon:4.36617, src:"St-Geniès-de-Malgoirès", srclat:43.9502, srclon:4.21494, dur:872 },
	my $url = sprintf($osmr, $stop->{dstlon}, $stop->{dstlat}, $stop->{srclon}, $stop->{scrlat});
	print "$url\n";

	my $json = get( $url );
	my $decoded_json = decode_json( $json );
	print Dumper $decoded_json;
	exit;
}
