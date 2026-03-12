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
            'dummy.png',
            bless({}, 'LocalNonCallable0139')
        );
        1;
    };
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader load_file accepted non-callable callback object'
        if $accepted;
    1;
};

ok($ok, 'loader load_file rejects non-callable callback object');
diag($@) if !$ok && $@ ne '';
