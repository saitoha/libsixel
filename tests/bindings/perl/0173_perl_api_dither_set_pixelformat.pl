#!/usr/bin/env perl

use strict;
use warnings FATAL => 'all';
use Test::More;
use Image::LibSIXEL;
use Image::LibSIXEL::Constants;

plan tests => 1;

my $ok = eval {
    my $dither = Image::LibSIXEL::sixel_dither_new(2, undef);
    Image::LibSIXEL::sixel_dither_set_pixelformat(
        $dither, Image::LibSIXEL::Constants::SIXEL_PIXELFORMAT_RGB888()
    );
    Image::LibSIXEL::sixel_dither_unref($dither);
    1;
};

ok($ok, 'dither pixelformat setter accepts its argument');
diag($@) if !$ok && $@ ne '';
