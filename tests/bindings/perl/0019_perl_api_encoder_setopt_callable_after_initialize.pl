#!/usr/bin/env perl

use strict;
use warnings;

use File::Spec;
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
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT(), File::Spec->devnull());
    1;
};

ok($ok, 'encoder setopt is callable after initialize');
diag($@) if !$ok && $@ ne '';
