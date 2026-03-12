#!/usr/bin/env perl

use strict;
use warnings;

use File::Spec;
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
    my $status = Image::LibSIXEL::_sixel_encoder_new(
        \$encoder,
        undef
    );
    die "sixel_encoder_new failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    $status = Image::LibSIXEL::_sixel_encoder_setopt(
        $encoder,
        ord(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT()),
        File::Spec->devnull()
    );
    die "sixel_encoder_setopt failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    my $pixels = pack('C*', 0, 1, 2, 3, 0, 1, 2, 3);
    my $palette = pack('C*',
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255
    );
    $status = Image::LibSIXEL::sixel_encoder_encode_bytes(
        $encoder,
        unpack('J', pack('p', $pixels)),
        4,
        2,
        Image::LibSIXEL::Constants::SIXEL_PIXELFORMAT_PAL8(),
        unpack('J', pack('p', $palette)),
        length($palette)
    );
    Image::LibSIXEL::_sixel_encoder_unref($encoder);
    die "sixel_encoder_encode_bytes failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    1;
};

ok($ok, 'encoder encode_bytes accepts bytes palette payload');
diag($@) if !$ok && $@ ne '';
