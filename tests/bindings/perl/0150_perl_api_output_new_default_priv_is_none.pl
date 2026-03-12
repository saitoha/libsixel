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
    my $saw_undef_priv = 0;
    my $output = Image::LibSIXEL::sixel_output_new(
        sub {
            my ($chunk, $size, $priv) = @_;
            $saw_undef_priv = 1
                if !defined($priv) && defined($chunk) && defined($size);
            return 0;
        }
    );
    my $dither = Image::LibSIXEL::sixel_dither_get(
        Image::LibSIXEL::Constants::SIXEL_BUILTIN_XTERM256()
    );
    my $depth = Image::LibSIXEL::sixel_helper_compute_depth(
        Image::LibSIXEL::Constants::SIXEL_PIXELFORMAT_RGB888()
    );
    my $status = Image::LibSIXEL::sixel_encode(
        pack('C*', 255, 0, 0),
        1,
        1,
        $depth,
        $dither,
        $output
    );
    Image::LibSIXEL::sixel_output_unref($output);
    Image::LibSIXEL::sixel_dither_unref($dither);
    die "sixel_encode failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    die 'output callback did not receive undef priv by default'
        if !$saw_undef_priv;
    1;
};

ok($ok, 'output callback default priv is undef when omitted');
diag($@) if !$ok && $@ ne '';
