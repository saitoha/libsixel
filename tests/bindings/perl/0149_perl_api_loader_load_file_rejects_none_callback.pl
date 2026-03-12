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
        Image::LibSIXEL::sixel_loader_load_file($loader, 'dummy.png', undef);
        1;
    };
    my $error = $@;
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader load_file accepted undef callback'
        if $accepted;
    die 'loader load_file did not report callback type validation'
        if $error !~ /callback must be CODE/;
    1;
};

ok($ok, 'loader load_file rejects undef callback');
diag($@) if !$ok && $@ ne '';
