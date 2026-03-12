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
            {},
            sub { return 0; }
        );
        1;
    };
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader accepted hashref filename unexpectedly'
        if $accepted;
    1;
};

ok($ok, 'loader hashref filename rejection path verified');
diag($@) if !$ok && $@ ne '';
