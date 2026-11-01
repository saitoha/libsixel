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
        Image::LibSIXEL::sixel_loader_setopt($loader, 'loader-order', 'builtin');
        1;
    } or $rejected = 1;
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader accepted non-numeric option identifier' if !$rejected;
    1;
};

ok($ok, 'loader setopt rejects non-numeric option identifier');
diag($@) if !$ok && $@ ne '';
