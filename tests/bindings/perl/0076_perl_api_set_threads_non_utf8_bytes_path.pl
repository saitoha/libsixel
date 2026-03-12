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
    Image::LibSIXEL::sixel_set_threads(pack('C', 0xFF));
    1;
};

ok($ok, 'set_threads non-UTF-8 bytes path is handled in current perl binding');
diag($@) if !$ok && $@ ne '';
