#!/usr/bin/env perl

use strict;
use warnings FATAL => 'all';
use Test::More;
use Image::LibSIXEL;
use FFI::Platypus::Buffer qw(scalar_to_buffer buffer_to_scalar);

plan tests => 1;

my $ok = eval {
    my $dither = Image::LibSIXEL::sixel_dither_new(2, undef);
    my $input = pack('C*', 0, 127, 255, 255, 0, 128);
    my ($pointer) = scalar_to_buffer($input);
    Image::LibSIXEL::sixel_dither_set_palette($dither, $pointer);
    # The borrowed pointer is binary data, not a NUL-terminated string.
    # Copy the exact palette length before releasing its owning dither.
    my $actual = buffer_to_scalar(
        Image::LibSIXEL::sixel_dither_get_palette($dither), 6
    );
    Image::LibSIXEL::sixel_dither_unref($dither);
    die 'palette bytes changed during setter/getter round trip'
        if $actual ne pack('C*', 0, 127, 255, 255, 0, 128);
    1;
};

ok($ok, 'palette round trip preserves embedded NUL and high bytes');
diag($@) if !$ok && $@ ne '';
