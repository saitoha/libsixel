#!/usr/bin/env perl

use strict;
use warnings;

use Test::More;

my $loaded = eval {
    require Image::LibSIXEL;
    1;
};
if (!$loaded) {
    plan skip_all => "libsixel perl binding failed to load: $@";
}

if (!eval {
    my $output = Image::LibSIXEL::sixel_output_new(sub { return 0; }, undef, undef);
    Image::LibSIXEL::sixel_output_unref($output);
    1;
}) {
    plan skip_all => "output callback API is unavailable: $@";
}

plan tests => 1;

my $ok = eval {
    my $output = Image::LibSIXEL::sixel_output_new(sub { return 0; }, undef, undef);
    Image::LibSIXEL::sixel_output_set_8bit_availability($output, 1);
    my $result = Image::LibSIXEL::sixel_output_get_8bit_availability($output);
    Image::LibSIXEL::sixel_output_unref($output);
    die "output get_8bit returned invalid state: $result"
        if $result != 0 && $result != 1;
    1;
};

ok($ok, 'output get_8bit returns valid availability state');
diag($@) if !$ok && $@ ne '';
