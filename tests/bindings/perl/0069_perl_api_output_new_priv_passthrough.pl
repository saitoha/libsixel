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
    my $seen = 0;
    my $priv = { marker => 'priv-marker' };
    my $output = Image::LibSIXEL::sixel_output_new(
        sub {
            my ($chunk, $size, $cb_priv) = @_;
            die 'output callback priv was not preserved'
                if !defined($cb_priv) || $cb_priv != $priv;
            $seen = 1 if defined($chunk) || defined($size);
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
    my $status = Image::LibSIXEL::sixel_encode(
        pack('C*', 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255),
        2,
        2,
        $depth,
        $dither,
        $output
    );
    Image::LibSIXEL::sixel_dither_unref($dither);
    Image::LibSIXEL::sixel_output_unref($output);
    die "sixel_encode failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    die 'output callback was not invoked'
        if !$seen;
    1;
};

ok($ok, 'output callback receives priv payload from output constructor');
diag($@) if !$ok && $@ ne '';
