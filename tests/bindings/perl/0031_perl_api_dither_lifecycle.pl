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
    my $dither = Image::LibSIXEL::sixel_dither_new(16, undef);
    Image::LibSIXEL::sixel_dither_ref($dither);
    Image::LibSIXEL::sixel_dither_unref($dither);
    Image::LibSIXEL::sixel_dither_unref($dither);
    1;
};

ok($ok, 'dither lifecycle APIs are callable');
diag($@) if !$ok && $@ ne '';
