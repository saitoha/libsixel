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
    my $decoder = Image::LibSIXEL::Decoder->new();
    die 'decoder initialize did not return usable instance'
        if !defined($decoder) || ref($decoder) ne 'Image::LibSIXEL::Decoder';
    1;
};

ok($ok, 'decoder initialize returns usable instance');
diag($@) if !$ok && $@ ne '';
