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
    my $encoder = Image::LibSIXEL::Encoder->new();
    die 'encoder initialize did not return usable instance'
        if !defined($encoder) || ref($encoder) ne 'Image::LibSIXEL::Encoder';
    1;
};

ok($ok, 'encoder initialize returns usable instance');
diag($@) if !$ok && $@ ne '';
