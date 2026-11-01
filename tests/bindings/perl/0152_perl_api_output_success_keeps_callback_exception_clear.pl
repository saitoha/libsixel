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

if (!eval {
    my $output = Image::LibSIXEL::sixel_output_new(sub { return 0; }, undef, undef);
    Image::LibSIXEL::sixel_output_unref($output);
    1;
}) {
    plan skip_all => "output callback API is unavailable: $@";
}

plan tests => 1;

my $ok = eval {
    my $calls = 0;
    my $output = Image::LibSIXEL::sixel_output_new(
        sub {
            $calls++;
            return 0;
        },
        undef,
        undef
    );
    my $dither = Image::LibSIXEL::sixel_dither_get(
        Image::LibSIXEL::Constants::SIXEL_BUILTIN_XTERM256()
    );
    my $depth = Image::LibSIXEL::sixel_helper_compute_depth(
        Image::LibSIXEL::Constants::SIXEL_PIXELFORMAT_RGB888()
    );
    my $first = Image::LibSIXEL::sixel_encode(
        pack('C*', 255, 0, 0),
        1,
        1,
        $depth,
        $dither,
        $output
    );
    my $second = Image::LibSIXEL::sixel_encode(
        pack('C*', 255, 0, 0),
        1,
        1,
        $depth,
        $dither,
        $output
    );
    Image::LibSIXEL::sixel_output_unref($output);
    Image::LibSIXEL::sixel_dither_unref($dither);
    die "first sixel_encode failed: $first"
        if $first != Image::LibSIXEL::Constants::SIXEL_OK();
    die "second sixel_encode failed: $second"
        if $second != Image::LibSIXEL::Constants::SIXEL_OK();
    die 'successful output callback did not run twice'
        if $calls < 2;
    1;
};

ok($ok, 'successful output callback remains clear across repeated encode');
diag($@) if !$ok && $@ ne '';
