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
    Image::LibSIXEL::sixel_set_encoder_threads(pack('C*', unpack('C*', 'all')));
    1;
};

ok($ok, 'set_encoder_threads accepts all keyword as bytes-like scalar');
diag($@) if !$ok && $@ ne '';
