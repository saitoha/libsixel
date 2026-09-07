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
        Image::LibSIXEL::Constants::SIXEL_OPTFLAG_QUANTIZE_MODEL(),
        'auto:sampling_policy=adaptive-grid:binning_policy=hard'
    );
    $encoder->setopt(
        Image::LibSIXEL::Constants::SIXEL_OPTFLAG_BACKGROUND_POLICY(),
        'explicit_first'
    );
    1;
};

ok($ok, 'encoder accepts nested palette policy suboptions');
diag($@) if !$ok && $@ ne '';
