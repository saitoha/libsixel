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
    {
        package LocalCallbackBoom0148;
    }
    my $output = Image::LibSIXEL::sixel_output_new(
        sub { die bless({ msg => 'typed boom' }, 'LocalCallbackBoom0148'); },
        undef,
        undef
    );
    my $dither = Image::LibSIXEL::sixel_dither_get(
        Image::LibSIXEL::Constants::SIXEL_BUILTIN_XTERM256()
    );
    my $depth = Image::LibSIXEL::sixel_helper_compute_depth(
        Image::LibSIXEL::Constants::SIXEL_PIXELFORMAT_RGB888()
    );
    my $error;
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
    } or $error = $@;
    Image::LibSIXEL::sixel_output_unref($output);
    Image::LibSIXEL::sixel_dither_unref($dither);
    die 'custom callback exception type was not preserved'
        if !ref($error) || ref($error) ne 'LocalCallbackBoom0148';
    die 'custom callback exception payload changed unexpectedly'
        if !exists($error->{msg}) || $error->{msg} ne 'typed boom';
    1;
};

ok($ok, 'output callback preserves custom exception type and payload');
diag($@) if !$ok && $@ ne '';
