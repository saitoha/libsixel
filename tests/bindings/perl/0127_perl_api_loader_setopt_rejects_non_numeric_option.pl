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
        'loader-order',
        'stb,png'
    );
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader accepted non-numeric option identifier'
        if $status == Image::LibSIXEL::Constants::SIXEL_OK();
    1;
};

ok($ok, 'loader setopt rejects non-numeric option identifier');
diag($@) if !$ok && $@ ne '';
