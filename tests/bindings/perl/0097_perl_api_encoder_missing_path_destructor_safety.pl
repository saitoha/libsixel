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
    {
        my $encoder = Image::LibSIXEL::Encoder->new();
        my $accepted = eval {
            $encoder->encode('tests/data/inputs/formats/this_file_does_not_exist.png');
            1;
        };
        die 'encoder accepted missing input path'
            if $accepted;
    }
    1;
};

ok($ok, 'encoder destructor stays stable after missing-path encode failure');
diag($@) if !$ok && $@ ne '';
