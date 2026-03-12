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

plan tests => 1;

my $ok = eval {
    my $encoder = Image::LibSIXEL::Encoder->new();
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_COLORS(), '16');
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_DIFFUSION(), 'atkinson');
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_PALETTE_TYPE(), 'hls');
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_QUALITY(), 'high');
    1;
};

ok($ok, 'encoder palette-related options are accepted');
diag($@) if !$ok && $@ ne '';
