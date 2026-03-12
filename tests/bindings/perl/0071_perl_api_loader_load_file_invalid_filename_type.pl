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
        Image::LibSIXEL::sixel_loader_load_file($loader, 123, sub { return 0; });
        1;
    };
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader accepted non-string filename type'
        if $accepted;
    1;
};

ok($ok, 'loader load_file rejects non-string filename type path');
diag($@) if !$ok && $@ ne '';
