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
    local $SIG{__WARN__} = sub { };
    Image::LibSIXEL::sixel_set_threads(pack('C*', unpack('C*', '  auto  ')));
    1;
};

ok($ok, 'set_threads accepts whitespace-padded auto bytes-like input');
diag($@) if !$ok && $@ ne '';
