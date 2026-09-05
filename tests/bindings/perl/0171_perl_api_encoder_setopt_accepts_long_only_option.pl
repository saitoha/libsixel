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
    $encoder->setopt(
        Image::LibSIXEL::Constants::SIXEL_OPTFLAG_SAMPLING_POLICY(),
        'adaptive-grid'
    );
    $encoder->setopt(
        Image::LibSIXEL::Constants::SIXEL_OPTFLAG_BINNING_POLICY(),
        'hard'
    );
    1;
};

ok($ok, 'encoder accepts a numeric long-only option flag');
diag($@) if !$ok && $@ ne '';
