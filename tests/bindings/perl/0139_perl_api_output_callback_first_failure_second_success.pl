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
    my $count = 0;
    my $output = Image::LibSIXEL::sixel_output_new(
        sub {
            $count++;
            die 'first callback failure' if $count == 1;
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
    my $first_failed = 0;
    eval {
        Image::LibSIXEL::sixel_encode(
            pack('C*', 255, 0, 0),
            1,
            1,
            $depth,
            $dither,
            $output
        );
        1;
    } or $first_failed = 1;
    my $status = Image::LibSIXEL::sixel_encode(
        pack('C*', 0, 255, 0),
        1,
        1,
        $depth,
        $dither,
        $output
    );
    Image::LibSIXEL::sixel_output_unref($output);
    Image::LibSIXEL::sixel_dither_unref($dither);
    die 'first encode did not fail on callback exception'
        if !$first_failed;
    die "second encode failed unexpectedly: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    die 'callback was not invoked on second encode'
        if $count < 2;
    1;
};

ok($ok, 'output callback first failure does not break second encode');
diag($@) if !$ok && $@ ne '';
