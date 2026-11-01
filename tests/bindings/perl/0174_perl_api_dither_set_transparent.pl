#!/usr/bin/env perl

use strict;
use warnings FATAL => 'all';
use Test::More;
use Image::LibSIXEL;

plan tests => 1;

my $ok = eval {
    my $dither = Image::LibSIXEL::sixel_dither_new(2, undef);
    Image::LibSIXEL::sixel_dither_set_transparent(
        $dither, 0
    );
    Image::LibSIXEL::sixel_dither_unref($dither);
    1;
};

ok($ok, 'dither transparent setter accepts its argument');
diag($@) if !$ok && $@ ne '';
