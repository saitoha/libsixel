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
    die 'sixel_dither_get returned null'
        if !defined($dither) || !$dither;
    my $palette_colors = Image::LibSIXEL::sixel_dither_get_num_of_palette_colors($dither);
    die 'built-in dither returned no palette colors'
        if $palette_colors <= 0;
    Image::LibSIXEL::sixel_dither_unref($dither);
    1;
};

ok($ok, 'built-in dither returns usable context');
diag($@) if !$ok && $@ ne '';
