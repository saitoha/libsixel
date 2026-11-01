#!/usr/bin/env perl

use strict;
use warnings;

use Test::More;

my $loaded = eval {
    require Image::LibSIXEL::Constants;
    1;
};
if (!$loaded) {
    plan tests => 1;
    fail('version constants are synchronized with configured header');
    diag("libsixel perl constants failed to load: $@");
    exit 0;
}

plan tests => 1;

my $source_root = $ENV{TOP_SRCDIR} // '.';
my $build_root = $ENV{TOP_BUILDDIR} // $source_root;
my @headers = (
    "$build_root/include/sixel.h",
    "$source_root/include/sixel.h",
    "$source_root/include/sixel.h.in",
);
my $header;
my %expected;
my %actual;
my @mismatched;
my $line;
my $fh;

for my $candidate (@headers) {
    if (-f $candidate) {
        $header = $candidate;
        last;
    }
}

if (!defined $header) {
    fail('version constants are synchronized with configured header');
    diag('public header was not found');
    exit 0;
}

open $fh, '<', $header or die "cannot open $header: $!";
while ($line = <$fh>) {
    if ($line =~ /^#define\s+(LIBSIXEL_(?:VERSION|ABI_VERSION))\s+"([^"]+)"/) {
        $expected{$1} = $2;
    }
}
close $fh;

%actual = (
    LIBSIXEL_VERSION => Image::LibSIXEL::Constants::LIBSIXEL_VERSION(),
    LIBSIXEL_ABI_VERSION => Image::LibSIXEL::Constants::LIBSIXEL_ABI_VERSION(),
);

for my $name (sort keys %expected) {
    my $actual_value = exists $actual{$name} ? $actual{$name} : '<missing>';
    if ($actual_value ne $expected{$name}) {
        push @mismatched,
            "$name=$actual_value, expected $expected{$name}";
    }
}

ok(
    keys(%expected) == 2 && @mismatched == 0,
    'version constants are synchronized with configured header'
);
diag('version constants were not found in header') if keys(%expected) != 2;
diag(join('; ', @mismatched)) if @mismatched;
