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
    my $dither = Image::LibSIXEL::sixel_dither_create(16);
    die 'sixel_dither_create returned null pointer'
        if !defined($dither) || !$dither;
    Image::LibSIXEL::sixel_dither_destroy($dither);
    1;
};

ok($ok, 'dither destroy API is callable');
diag($@) if !$ok && $@ ne '';
