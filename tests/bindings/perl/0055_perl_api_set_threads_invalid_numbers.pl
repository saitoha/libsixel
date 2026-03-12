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
    Image::LibSIXEL::sixel_set_threads(0);
    Image::LibSIXEL::sixel_set_threads(-1);
    Image::LibSIXEL::sixel_set_threads(1);
    1;
};

ok($ok, 'set_threads handles non-positive numeric inputs in current perl path');
diag($@) if !$ok && $@ ne '';
