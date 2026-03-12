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
    Image::LibSIXEL::sixel_set_threads('2');
    Image::LibSIXEL::sixel_set_threads('auto');
    local $SIG{__WARN__} = sub { };
    Image::LibSIXEL::sixel_set_threads('bad');
    1;
};

ok($ok, 'set_threads accepts byte-like scalar inputs in current perl path');
diag($@) if !$ok && $@ ne '';
