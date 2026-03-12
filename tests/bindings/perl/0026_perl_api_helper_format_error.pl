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
    my $message = Image::LibSIXEL::_sixel_helper_format_error(
        Image::LibSIXEL::Constants::SIXEL_RUNTIME_ERROR()
    );
    die 'helper format_error returned an empty message'
        if !defined($message) || $message eq '';
    1;
};

ok($ok, 'helper format_error returns a non-empty message');
diag($@) if !$ok && $@ ne '';
