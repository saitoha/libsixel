#!/usr/bin/env perl

use strict;
use warnings FATAL => 'all';
use Test::More;
use Image::LibSIXEL;
use FFI::Platypus::Buffer qw(scalar_to_buffer buffer_to_scalar);

plan tests => 1;

my $ok = eval {
    my $dither = Image::LibSIXEL::sixel_dither_new(2, undef);
    my $input = pack('C*', 1, 2, 3, 4, 5, 6);
    my ($pointer) = scalar_to_buffer($input);
    Image::LibSIXEL::sixel_dither_set_palette($dither, $pointer);
    # Modify the caller-owned storage after the setter has copied it.
    substr($input, 0, 1) = "\xff";
    my $actual = buffer_to_scalar(
        Image::LibSIXEL::sixel_dither_get_palette($dither), 6
    );
    Image::LibSIXEL::sixel_dither_unref($dither);
    die 'palette retained caller storage instead of copying bytes'
        if $actual ne pack('C*', 1, 2, 3, 4, 5, 6);
    1;
};

ok($ok, 'dither palette setter copies caller-owned bytes');
diag($@) if !$ok && $@ ne '';
