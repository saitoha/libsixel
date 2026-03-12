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
        pack('C*', 0, 0, 0, 255, 255, 255)
    );
    my $palette = Image::LibSIXEL::sixel_dither_get_palette($dither);
    my $colors = Image::LibSIXEL::sixel_dither_get_num_of_palette_colors($dither);
    Image::LibSIXEL::sixel_dither_unref($dither);
    die 'dither palette getter returned null pointer'
        if !defined($palette) || !$palette;
    die "dither palette color count was not updated: $colors"
        if $colors < 2;
    1;
};

ok($ok, 'dither set_palette and palette getter are callable');
diag($@) if !$ok && $@ ne '';
