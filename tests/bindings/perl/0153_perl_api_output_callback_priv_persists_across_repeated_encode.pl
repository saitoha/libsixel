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
    my $priv = {};
    my $calls = 0;
    my $saw_same_priv = 1;
    my $output = Image::LibSIXEL::sixel_output_new(
        sub {
            my ($chunk, $size, $cb_priv) = @_;
            $calls++;
            $saw_same_priv = 0 if !defined($cb_priv) || $cb_priv != $priv;
            return 0;
        },
        $priv,
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
    die 'output callback was not invoked for both encodes'
        if $calls < 2;
    die 'output callback priv was not preserved across encodes'
        if !$saw_same_priv;
    1;
};

ok($ok, 'output callback priv persists across repeated encode');
diag($@) if !$ok && $@ ne '';
