#!/usr/bin/env perl

use strict;
use warnings;

use File::Spec;
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
    my $source = File::Spec->catfile($ENV{TOP_SRCDIR}, 'tests', 'data', 'inputs', 'snake_64.png');
    my $loader = Image::LibSIXEL::sixel_loader_new(undef);
    my $accepted = eval {
        Image::LibSIXEL::sixel_loader_load_file(
            $loader,
            $source,
            []
        );
        1;
    };
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader accepted list callback unexpectedly'
        if $accepted;
    1;
};

ok($ok, 'loader list callback rejection path verified');
diag($@) if !$ok && $@ ne '';
