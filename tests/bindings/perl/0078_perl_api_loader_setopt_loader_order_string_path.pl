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
    Image::LibSIXEL::sixel_loader_setopt(
        $loader,
        Image::LibSIXEL::Constants::SIXEL_LOADER_OPTION_LOADER_ORDER(),
        'builtin'
    );
    Image::LibSIXEL::sixel_loader_unref($loader);
    1;
};

ok($ok, 'loader setopt accepts loader-order string path safely');
diag($@) if !$ok && $@ ne '';
