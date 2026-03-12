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
    my $encoder;
    my $status = Image::LibSIXEL::_sixel_encoder_new(\$encoder, undef);
    die "sixel_encoder_new failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    Image::LibSIXEL::sixel_encoder_ref($encoder);
    Image::LibSIXEL::_sixel_encoder_unref($encoder);
    Image::LibSIXEL::_sixel_encoder_unref($encoder);
    1;
};

ok($ok, 'raw encoder ref and unref APIs are callable');
diag($@) if !$ok && $@ ne '';
