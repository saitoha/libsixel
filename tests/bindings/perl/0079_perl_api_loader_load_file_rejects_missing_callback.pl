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
    my $source = File::Spec->catfile(
        $ENV{TOP_SRCDIR} // '.',
        qw(tests data inputs snake_64.png)
    );
    my $loader = Image::LibSIXEL::sixel_loader_new(undef);
    my $accepted = eval {
        Image::LibSIXEL::sixel_loader_load_file($loader, $source, undef);
        1;
    };
    my $error = $@;
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader load_file accepted missing callback'
        if $accepted;
    die 'loader missing-callback error message was not returned'
        if $error !~ /callback must be CODE/;
    1;
};

ok($ok, 'loader load_file rejects missing callback');
diag($@) if !$ok && $@ ne '';
