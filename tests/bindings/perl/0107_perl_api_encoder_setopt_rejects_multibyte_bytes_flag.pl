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
        pack('C*', 112, 112),
        '256'
    );
    Image::LibSIXEL::_sixel_encoder_unref($encoder);
    die 'raw encoder_setopt accepted multi-byte bytes option flag input'
        if $status == Image::LibSIXEL::Constants::SIXEL_OK();
    1;
};

ok($ok, 'raw encoder_setopt rejects multi-byte bytes option flag input');
diag($@) if !$ok && $@ ne '';
