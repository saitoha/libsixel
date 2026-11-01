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
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_WIDTH(), '96');
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_HEIGHT(), 'auto');
    1;
};

ok($ok, 'encoder aspect resize options are accepted');
diag($@) if !$ok && $@ ne '';
