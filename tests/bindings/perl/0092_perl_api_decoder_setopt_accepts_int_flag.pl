#!/usr/bin/env perl

use strict;
use warnings FATAL => 'all';
use Test::More;
use Image::LibSIXEL;

plan tests => 1;

my $ok = eval {
    my $decoder = Image::LibSIXEL::Decoder->new();
    # 111 is the numeric C option ID for the output flag, 'o'.
    $decoder->setopt(111, 'dummy.png');
    1;
};

ok($ok, 'decoder setopt accepts a valid integer option ID');
diag($@) if !$ok && $@ ne '';
