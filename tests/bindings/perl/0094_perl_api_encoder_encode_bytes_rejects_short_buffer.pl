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
    $status = Image::LibSIXEL::sixel_encoder_encode_bytes(
        $encoder,
        0,
        2,
        2,
        Image::LibSIXEL::Constants::SIXEL_PIXELFORMAT_RGB888(),
        undef,
        0
    );
    Image::LibSIXEL::_sixel_encoder_unref($encoder);
    die 'encoder encode_bytes accepted null/short pixel buffer input'
        if $status == Image::LibSIXEL::Constants::SIXEL_OK();
    1;
};

ok($ok, 'encoder encode_bytes rejects null/short pixel buffer input');
diag($@) if !$ok && $@ ne '';
