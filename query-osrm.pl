#!/usr/bin/perl
use strict;
use warnings;
use v5.10;
use LWP::Simple;                # From CPAN
use JSON qw( decode_json encode_json );     # From CPAN
use Data::Dumper;               # Perl core module

my $osmr = "http://localhost:5000/route/v1/transit/%f,%f;%f,%f?overview=full&geometries=geojson";

if(!$ARGV[0]) {
	die "./query-osrm.pl <json output of parser>\n";
}

# Open parser output
my $text = `cat "$ARGV[0]"`;
$text =~ s/:/=>/smg; #Output of the parser is for JS, but easy to transform to perl code
my $stops = eval($text);


#print "{ 'type': 'geojson', 'data': { 'type': 'FeatureCollection', 'features': [ \n";
say '{ "features": [';

# Foreach stop
my $is_first = 1;
foreach my $stop (@$stops) { 
	#{ dst:"Nîmes", dstlat:43.8324, dstlon:4.36617, src:"St-Geniès-de-Malgoirès", srclat:43.9502, srclon:4.21494, dur:872 },
	my $url = sprintf($osmr, $stop->{dstlon}, $stop->{dstlat}, $stop->{srclon}, $stop->{srclat});

	my $json = get( $url );
	my $decoded_json = decode_json( $json );
	#print Dumper $decoded_json;
	
	my $coords = $decoded_json->{routes}->[0]->{geometry}->{coordinates};
	next if(!$coords);
	
	if(!$is_first) {
		print ",\n";
	} else {
		$is_first = 0;
	}
	print '{ "type": "Feature", "properties": { "dur": '.$stop->{dur}.' }, "geometry": { "coordinates": '.encode_json($coords).', "type": "LineString" } } ';
}
say '], "type": "FeatureCollection" }';
