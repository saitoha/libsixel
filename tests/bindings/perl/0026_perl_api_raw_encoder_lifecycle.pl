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
    my $status = Image::LibSIXEL::_sixel_encoder_new(\$encoder, undef);
    die "sixel_encoder_new failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    $status = Image::LibSIXEL::_sixel_encoder_setopt(
        $encoder,
        ord(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT()),
        File::Spec->devnull()
    );
    die "sixel_encoder_setopt failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    Image::LibSIXEL::_sixel_encoder_unref($encoder);
    1;
};

ok($ok, 'raw encoder APIs create, configure, and release successfully');
diag($@) if !$ok && $@ ne '';
