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
        Image::LibSIXEL::Constants::SIXEL_LOADER_OPTION_LOADER_ORDER(),
        pack('C*', unpack('C*', 'builtin'))
    );
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader rejected bytearray-like loader-order input'
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    1;
};

ok($ok, 'loader setopt accepts bytearray-like loader-order input');
diag($@) if !$ok && $@ ne '';
