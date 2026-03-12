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
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_WIDTH(), '64');
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_HEIGHT(), '32');
    1;
};

ok($ok, 'encoder fixed resize options are accepted');
diag($@) if !$ok && $@ ne '';
