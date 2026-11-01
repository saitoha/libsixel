#!/usr/bin/env perl

use strict;
use warnings FATAL => 'all';
use Test::More;
use Image::LibSIXEL;
use Image::LibSIXEL::Constants;
use FFI::Platypus::Buffer qw(buffer_to_scalar);

plan tests => 1;

my $ok = eval {
    my $dither = Image::LibSIXEL::sixel_dither_get(
        Image::LibSIXEL::Constants::SIXEL_BUILTIN_MONO_DARK()
    );
    Image::LibSIXEL::sixel_dither_set_complexion_score($dither, 1);
    my $palette = buffer_to_scalar(
        Image::LibSIXEL::sixel_dither_get_palette($dither), 6
    );
    Image::LibSIXEL::sixel_dither_unref($dither);
    die 'retired complexion setter changed palette bytes'
        if $palette ne pack('C*', 0, 0, 0, 255, 255, 255);
    1;
};

ok($ok, 'retired complexion setter preserves palette bytes');
diag($@) if !$ok && $@ ne '';
