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
    for my $setter (\&Image::LibSIXEL::sixel_set_encoder_threads,
                    \&Image::LibSIXEL::sixel_set_decoder_threads) {
        for my $value ('1.5', 1.5) {
            my $accepted = eval { $setter->($value); 1 };
            die "thread budget API accepted decimal inputs\n" if $accepted;
            die "unexpected validation error: $@" if
                $@ !~ /\Athreads must be a positive integer or 'all'/;
        }
    }
    1;
};

ok($ok, 'both thread budget setters reject decimal inputs');
diag($@) if !$ok && $@ ne '';
