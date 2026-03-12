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
    Image::LibSIXEL::sixel_dither_set_palette(
        $dither,
        pack('C*', 1, 2, 3, 4, 5, 6)
    );
    my $palette_ptr = Image::LibSIXEL::sixel_dither_get_palette($dither);
    Image::LibSIXEL::sixel_dither_unref($dither);
    die 'dither palette getter returned null pointer'
        if !defined($palette_ptr) || !$palette_ptr;
    1;
};

ok($ok, 'dither palette getter and setter APIs are callable');
diag($@) if !$ok && $@ ne '';
