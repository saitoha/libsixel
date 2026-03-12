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
    my $decoder;
    my $status = Image::LibSIXEL::_sixel_decoder_new(\$decoder, undef);
    die "sixel_decoder_new failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    Image::LibSIXEL::sixel_decoder_ref($decoder);
    Image::LibSIXEL::_sixel_decoder_unref($decoder);
    Image::LibSIXEL::_sixel_decoder_unref($decoder);
    1;
};

ok($ok, 'raw decoder ref and unref APIs are callable');
diag($@) if !$ok && $@ ne '';
