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
    my $dither = Image::LibSIXEL::sixel_dither_new(2, undef);
    Image::LibSIXEL::sixel_dither_set_palette($dither, pack('C*', 1, 2, 3, 4, 5, 6));
    my $palette = Image::LibSIXEL::sixel_dither_get_palette($dither);
    Image::LibSIXEL::sixel_dither_unref($dither);
    die 'dither get_palette did not return palette payload'
        if !defined($palette) || !$palette;
    1;
};

ok($ok, 'dither get_palette returns palette payload in current perl path');
diag($@) if !$ok && $@ ne '';
