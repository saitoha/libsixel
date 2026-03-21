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
    my $loader = Image::LibSIXEL::sixel_loader_new(undef);
    my $rejected = 0;
    eval {
        Image::LibSIXEL::sixel_loader_setopt(
            $loader,
            Image::LibSIXEL::Constants::SIXEL_LOADER_OPTION_REQCOLORS(),
            'abc'
        );
        1;
    } or $rejected = 1;
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader setopt accepted non-numeric reqcolors string'
        if !$rejected;
    1;
};

ok($ok, 'loader setopt rejects non-numeric reqcolors string');
diag($@) if !$ok && $@ ne '';
