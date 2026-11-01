#!/usr/bin/env perl

use strict;
use warnings;

use Test::More;

my $loaded = eval {
    require Image::LibSIXEL;
    1;
};
if (!$loaded) {
    plan skip_all => "libsixel perl binding failed to load: $@";
}

plan tests => 1;

my $ok = eval {
    my $decoder = Image::LibSIXEL::Decoder->new();
    $decoder->DESTROY();
    $decoder->DESTROY();
    1;
};

ok($ok, 'decoder unref path is idempotent');
diag($@) if !$ok && $@ ne '';
