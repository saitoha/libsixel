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
    {
        package LocalPathLike0154;
    }
    my $loader = Image::LibSIXEL::sixel_loader_new(undef);
    my $accepted = eval {
        Image::LibSIXEL::sixel_loader_load_file(
            $loader,
            bless({}, 'LocalPathLike0154'),
            sub { return 0; }
        );
        1;
    };
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader accepted non-coercible path-like filename unexpectedly'
        if $accepted;
    1;
};

ok($ok, 'loader rejects non-coercible path-like filename');
diag($@) if !$ok && $@ ne '';
