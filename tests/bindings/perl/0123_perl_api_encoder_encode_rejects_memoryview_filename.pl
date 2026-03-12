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
    my $filename = 'dummy.png';
    my $accepted = eval {
        $encoder->encode(\$filename);
        1;
    };
    die 'encoder accepted memoryview-like filename input'
        if $accepted;
    1;
};

ok($ok, 'encoder encode rejects memoryview-like filename input');
diag($@) if !$ok && $@ ne '';
