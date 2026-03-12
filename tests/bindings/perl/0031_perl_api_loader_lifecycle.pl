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
    my $loader = Image::LibSIXEL::sixel_loader_new(undef);
    Image::LibSIXEL::sixel_loader_ref($loader);
    Image::LibSIXEL::sixel_loader_unref($loader);
    Image::LibSIXEL::sixel_loader_unref($loader);
    1;
};

ok($ok, 'loader lifecycle APIs are callable');
diag($@) if !$ok && $@ ne '';
