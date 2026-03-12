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
    my $status = Image::LibSIXEL::_sixel_loader_load_file(
        $loader,
        [100, 117, 109, 109, 121, 46, 112, 110, 103],
        0
    );
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader load_file accepted bytearray-like filename input'
        if $status == Image::LibSIXEL::Constants::SIXEL_OK();
    1;
};

ok($ok, 'loader load_file rejects bytearray-like filename input');
diag($@) if !$ok && $@ ne '';
