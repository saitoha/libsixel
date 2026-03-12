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
    my $dither = Image::LibSIXEL::sixel_dither_get(
        Image::LibSIXEL::Constants::SIXEL_BUILTIN_XTERM256()
    );
    my $depth = Image::LibSIXEL::sixel_helper_compute_depth(
        Image::LibSIXEL::Constants::SIXEL_PIXELFORMAT_RGB888()
    );
    my $failing = Image::LibSIXEL::sixel_output_new(
        sub { die 'first boom'; },
        undef,
        undef
    );
    my $calls = 0;
    my $succeeding = Image::LibSIXEL::sixel_output_new(
        sub {
            $calls++;
            return 0;
        },
        undef,
        undef
    );
    my $first_failed = 0;
    eval {
        Image::LibSIXEL::sixel_encode(
            pack('C*', 255, 0, 0),
            1,
            1,
            $depth,
            $dither,
            $failing
        );
        1;
    } or $first_failed = 1;
    my $status = Image::LibSIXEL::sixel_encode(
        pack('C*', 0, 255, 0),
        1,
        1,
        $depth,
        $dither,
        $succeeding
    );
    Image::LibSIXEL::sixel_output_unref($succeeding);
    Image::LibSIXEL::sixel_output_unref($failing);
    Image::LibSIXEL::sixel_dither_unref($dither);
    die 'failing output did not surface callback exception'
        if !$first_failed;
    die "second output encode failed unexpectedly: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    die 'second output callback was not invoked'
        if $calls <= 0;
    1;
};

ok($ok, 'output callback failure is isolated per output handle');
diag($@) if !$ok && $@ ne '';
