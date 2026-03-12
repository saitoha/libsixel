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
    my $accepted = eval {
        Image::LibSIXEL::sixel_output_new(123, undef, undef);
        1;
    };
    die 'output_new accepted non-callable write_proc'
        if $accepted;
    1;
};

ok($ok, 'output_new rejects non-callable write_proc');
diag($@) if !$ok && $@ ne '';
