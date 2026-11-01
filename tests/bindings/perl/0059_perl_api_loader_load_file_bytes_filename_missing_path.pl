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
    my $loader = Image::LibSIXEL::sixel_loader_new(undef);
    my $accepted = eval {
        Image::LibSIXEL::sixel_loader_load_file(
            $loader,
            pack('C*', unpack('C*', '/definitely/missing/image.png')),
            sub { return 0; }
        );
        1;
    };
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader load_file unexpectedly succeeded on missing bytes path'
        if $accepted;
    1;
};

ok($ok, 'loader load_file handles bytes-like filename on missing path');
diag($@) if !$ok && $@ ne '';
