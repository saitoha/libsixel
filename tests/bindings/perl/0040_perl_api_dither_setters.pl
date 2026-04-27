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
    Image::LibSIXEL::sixel_dither_set_diffusion_type(
        $dither,
        Image::LibSIXEL::Constants::SIXEL_DIFFUSE_ATKINSON()
    );
    Image::LibSIXEL::sixel_dither_set_diffusion_scan(
        $dither,
        Image::LibSIXEL::Constants::SIXEL_SCAN_SERPENTINE()
    );
    Image::LibSIXEL::sixel_dither_set_body_only($dither, 0);
    Image::LibSIXEL::sixel_dither_set_optimize_palette($dither, 1);
    Image::LibSIXEL::sixel_dither_set_pixelformat(
        $dither,
        Image::LibSIXEL::Constants::SIXEL_PIXELFORMAT_RGB888()
    );
    Image::LibSIXEL::sixel_dither_set_transparent($dither, 0);
    Image::LibSIXEL::sixel_dither_unref($dither);
    1;
};

ok($ok, 'dither setter APIs accept expected argument values');
diag($@) if !$ok && $@ ne '';
