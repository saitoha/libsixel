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
    my $accepted = eval {
        Image::LibSIXEL::sixel_output_set_skip_header($output);
        1;
    };
    Image::LibSIXEL::sixel_output_unref($output);
    die 'output setter accepted missing argument'
        if $accepted;
    1;
};

ok($ok, 'output setter rejects missing argument');
diag($@) if !$ok && $@ ne '';
