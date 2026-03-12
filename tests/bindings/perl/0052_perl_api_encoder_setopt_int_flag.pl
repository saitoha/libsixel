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
    $status = Image::LibSIXEL::_sixel_encoder_setopt(
        $encoder,
        ord(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_COLORS()),
        '16'
    );
    Image::LibSIXEL::_sixel_encoder_unref($encoder);
    die "encoder setopt rejected integer option flag path: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    1;
};

ok($ok, 'encoder setopt accepts integer option flag path');
diag($@) if !$ok && $@ ne '';
