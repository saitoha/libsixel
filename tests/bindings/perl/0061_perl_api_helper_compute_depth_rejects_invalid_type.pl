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
    my $rejected = 0;
    local $SIG{__WARN__} = sub { die @_ };
    eval {
        Image::LibSIXEL::sixel_helper_compute_depth('not-a-number');
        1;
    } or $rejected = 1;
    die 'helper compute_depth accepted non-numeric pixelformat'
        if !$rejected;
    1;
};

ok($ok, 'helper compute_depth rejects non-numeric pixelformat');
diag($@) if !$ok && $@ ne '';
