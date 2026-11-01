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

plan tests => 1;

my $ok = eval {
    Image::LibSIXEL::sixel_set_encoder_threads('2');
    Image::LibSIXEL::sixel_set_encoder_threads('all');
    my $accepted = eval {
        Image::LibSIXEL::sixel_set_encoder_threads('bad');
        1;
    };
    die "set_encoder_threads accepted invalid bytes\n" if $accepted;
    die "unexpected validation error: $@" if
        $@ !~ /\Athreads must be a positive integer or 'all'/;
    1;
};

ok($ok, 'set_encoder_threads accepts valid bytes and rejects invalid bytes');
diag($@) if !$ok && $@ ne '';
