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
    Image::LibSIXEL::sixel_set_threads(undef);
    Image::LibSIXEL::sixel_set_threads(1);
    1;
};

ok($ok, 'set_threads none input path is handled in current perl binding');
diag($@) if !$ok && $@ ne '';
