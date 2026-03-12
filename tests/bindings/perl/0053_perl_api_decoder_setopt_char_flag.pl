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
    my $decoder = Image::LibSIXEL::Decoder->new();
    $decoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT(), 'dummy.png');
    1;
};

ok($ok, 'decoder setopt accepts character option flag');
diag($@) if !$ok && $@ ne '';
