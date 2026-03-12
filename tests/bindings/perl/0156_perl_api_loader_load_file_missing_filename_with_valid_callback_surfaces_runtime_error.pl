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
            '__missing__0156__.png',
            sub { return 0; }
        );
        1;
    };
    my $error = $@;
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader missing filename unexpectedly succeeded'
        if $accepted;
    die 'loader missing filename did not surface runtime error path'
        if $error =~ /callback must be CODE/;
    1;
};

ok($ok, 'loader missing filename runtime error path verified with valid callback');
diag($@) if !$ok && $@ ne '';
