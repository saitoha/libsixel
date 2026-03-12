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
    my $failed = 0;
    eval { $encoder->setopt('X', '16'); 1 } or $failed = 1;
    die 'encoder accepted unsupported option flag' if !$failed;
    1;
};

ok($ok, 'encoder rejects unsupported option flag');
diag($@) if !$ok && $@ ne '';
