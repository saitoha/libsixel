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
    my $encoder = Image::LibSIXEL::Encoder->new();
    $encoder->DESTROY();
    $encoder->DESTROY();
    1;
};

ok($ok, 'encoder unref path is idempotent');
diag($@) if !$ok && $@ ne '';
