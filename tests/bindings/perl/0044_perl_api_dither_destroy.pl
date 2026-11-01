#!/usr/bin/env perl

use strict;
use warnings FATAL => 'all';
use Test::More;
use Image::LibSIXEL;

plan tests => 1;

my $ok = eval {
    # Use the supported constructor so this case isolates the destroy API.
    my $dither = Image::LibSIXEL::sixel_dither_new(2, undef);
    Image::LibSIXEL::sixel_dither_destroy($dither);
    1;
};

ok($ok, 'dither destroy API is callable');
diag($@) if !$ok && $@ ne '';
