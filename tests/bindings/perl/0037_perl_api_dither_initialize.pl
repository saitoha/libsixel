#!/usr/bin/env perl

use strict;
use warnings;

use Test::More;

my $loaded = eval {
    require Image::LibSIXEL;
    require Image::LibSIXEL::Constants;
    1;
};
if (!$loaded) {
    plan skip_all => "libsixel perl binding failed to load: $@";
}

plan tests => 1;

my $ok = eval {
    my $dither = Image::LibSIXEL::sixel_dither_new(16, undef);
    my $pixels = pack('C*', 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255);
    my $pixels_ptr = unpack('J', pack('p', $pixels));
    Image::LibSIXEL::sixel_dither_initialize(
        $dither,
        $pixels_ptr,
        2,
        2,
        Image::LibSIXEL::Constants::SIXEL_PIXELFORMAT_RGB888(),
        Image::LibSIXEL::Constants::SIXEL_LARGE_AUTO(),
        Image::LibSIXEL::Constants::SIXEL_REP_AUTO(),
        Image::LibSIXEL::Constants::SIXEL_QUALITY_AUTO()
    );
    my $histogram = Image::LibSIXEL::sixel_dither_get_num_of_histogram_colors($dither);
    Image::LibSIXEL::sixel_dither_unref($dither);
    die 'dither initialize did not update histogram state'
        if $histogram <= 0;
    1;
};

ok($ok, 'dither initialize updates histogram state');
diag($@) if !$ok && $@ ne '';
