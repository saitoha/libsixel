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
    my $decoder = Image::LibSIXEL::Decoder->new();
    my $accepted = eval {
        $decoder->setopt([105], 'dummy.png');
        1;
    };
    die 'decoder accepted bytearray-like option flag input'
        if $accepted;
    1;
};

ok($ok, 'decoder setopt rejects bytearray-like option flag input');
diag($@) if !$ok && $@ ne '';
