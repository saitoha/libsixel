#!/usr/bin/env perl

use strict;
use warnings FATAL => 'all';
use Test::More;
use Image::LibSIXEL;
use Image::LibSIXEL::Constants;

plan tests => 1;

my $ok = eval {
    my $dither = Image::LibSIXEL::sixel_dither_new(2, undef);
    Image::LibSIXEL::sixel_dither_set_diffusion_scan(
        $dither, Image::LibSIXEL::Constants::SIXEL_SCAN_SERPENTINE()
    );
    Image::LibSIXEL::sixel_dither_unref($dither);
    1;
};

ok($ok, 'dither diffusion_scan setter accepts its argument');
diag($@) if !$ok && $@ ne '';
