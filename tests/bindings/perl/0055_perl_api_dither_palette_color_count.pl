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
    my $dither = Image::LibSIXEL::sixel_dither_get(
        Image::LibSIXEL::Constants::SIXEL_BUILTIN_XTERM256()
    );
    my $count = Image::LibSIXEL::sixel_dither_get_num_of_palette_colors($dither);
    Image::LibSIXEL::sixel_dither_unref($dither);
    die 'dither palette color count is not positive'
        if $count <= 0;
    1;
};

ok($ok, 'dither palette color count getter returns positive value');
diag($@) if !$ok && $@ ne '';
