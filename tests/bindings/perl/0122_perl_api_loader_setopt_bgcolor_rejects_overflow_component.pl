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
    my $status = Image::LibSIXEL::_sixel_loader_setopt(
        $loader,
        Image::LibSIXEL::Constants::SIXEL_LOADER_OPTION_BGCOLOR(),
        [0, 0, 'x']
    );
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader setopt returned non-numeric status for bgcolor component path'
        if !defined($status) || $status !~ /^-?[0-9]+$/;
    1;
};

ok($ok, 'loader setopt bgcolor component validation path is handled safely');
diag($@) if !$ok && $@ ne '';
