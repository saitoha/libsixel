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
    my $decoder = Image::LibSIXEL::Decoder->new();
    die 'encoder object was not created'
        if !defined($encoder) || ref($encoder) ne 'Image::LibSIXEL::Encoder';
    die 'decoder object was not created'
        if !defined($decoder) || ref($decoder) ne 'Image::LibSIXEL::Decoder';
    1;
};

ok($ok, 'perl bindings smoke lifecycle verified');
diag($@) if !$ok && $@ ne '';
