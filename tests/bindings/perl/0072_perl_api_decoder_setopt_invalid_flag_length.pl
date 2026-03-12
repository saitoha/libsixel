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
    my $decoder = Image::LibSIXEL::Decoder->new();
    my $accepted = eval {
        $decoder->setopt('xy', 'dummy.png');
        1;
    };
    die 'decoder accepted multi-character option flag'
        if $accepted;
    1;
};

ok($ok, 'decoder rejects multi-character option flag');
diag($@) if !$ok && $@ ne '';
