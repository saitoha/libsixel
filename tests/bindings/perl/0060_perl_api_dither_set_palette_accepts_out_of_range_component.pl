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
    my $dither = Image::LibSIXEL::sixel_dither_new(2, undef);
    local $SIG{__WARN__} = sub { };
    Image::LibSIXEL::sixel_dither_set_palette(
        $dither,
        pack('C*', 0, 0, 0, 256, 0, 0)
    );
    Image::LibSIXEL::sixel_dither_unref($dither);
    1;
};

ok($ok, 'dither set_palette accepts out-of-range component in current perl path');
diag($@) if !$ok && $@ ne '';
