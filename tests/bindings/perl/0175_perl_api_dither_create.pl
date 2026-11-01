#!/usr/bin/env perl

use strict;
use warnings FATAL => 'all';
use Test::More;
use Image::LibSIXEL;

plan tests => 1;

my $ok = eval {
    my $dither = Image::LibSIXEL::sixel_dither_create(2);
    die 'deprecated dither constructor returned a null pointer'
        if !defined($dither) || !$dither;
    Image::LibSIXEL::sixel_dither_unref($dither);
    1;
};

ok($ok, 'deprecated dither constructor remains callable');
diag($@) if !$ok && $@ ne '';
