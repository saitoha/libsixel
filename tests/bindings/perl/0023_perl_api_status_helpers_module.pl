#!/usr/bin/env perl

use strict;
use warnings;

use Test::More;

my $loaded = eval {
    require Image::LibSIXEL;
    require Image::LibSIXEL::Constants;
    1;
};
if (!$loaded) {
    plan skip_all => "libsixel perl binding failed to load: $@";
}

plan tests => 1;

my $ok = eval {
    my $status_ok = eval {
        Image::LibSIXEL::_croak_on_status(Image::LibSIXEL::Constants::SIXEL_OK());
        1;
    };
    die 'status helper rejected SIXEL_OK'
        if !$status_ok;

    for my $status (
        Image::LibSIXEL::Constants::SIXEL_FALSE(),
        Image::LibSIXEL::Constants::SIXEL_RUNTIME_ERROR(),
        Image::LibSIXEL::Constants::SIXEL_BAD_ALLOCATION()
    ) {
        my $status_failed = eval {
            Image::LibSIXEL::_croak_on_status($status);
            1;
        };
        die "status helper accepted failure status $status"
            if $status_failed;
        die "status helper failure status $status did not provide error text"
            if !$@;
    }
    1;
};

ok($ok, 'status helper path classifies success and failure statuses');
diag($@) if !$ok && $@ ne '';
