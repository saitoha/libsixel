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
    my $accepted = eval {
        $encoder->setopt('', '16');
        1;
    };
    die 'encoder accepted empty option flag'
        if $accepted;
    1;
};

ok($ok, 'encoder rejects empty option flag');
diag($@) if !$ok && $@ ne '';
