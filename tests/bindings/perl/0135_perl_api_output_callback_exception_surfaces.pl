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
    my $output = Image::LibSIXEL::sixel_output_new(
        sub { die 'callback boom'; },
        undef,
        undef
    );
    my $dither = Image::LibSIXEL::sixel_dither_get(
        Image::LibSIXEL::Constants::SIXEL_BUILTIN_XTERM256()
    );
    my $depth = Image::LibSIXEL::sixel_helper_compute_depth(
        Image::LibSIXEL::Constants::SIXEL_PIXELFORMAT_RGB888()
    );
    my $raised = 0;
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
    } or $raised = 1;
    Image::LibSIXEL::sixel_output_unref($output);
    Image::LibSIXEL::sixel_dither_unref($dither);
    die 'output callback exception did not surface'
        if !$raised;
    1;
};

ok($ok, 'output callback exception surfaces in perl path');
diag($@) if !$ok && $@ ne '';
