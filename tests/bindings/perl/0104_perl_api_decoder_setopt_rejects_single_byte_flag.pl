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
    my $decoder;
    my $status = Image::LibSIXEL::_sixel_decoder_new(
        \$decoder,
        undef
    );
    die "sixel_decoder_new failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    my $status_bytes = Image::LibSIXEL::_sixel_decoder_setopt(
        $decoder,
        pack('C*', unpack('C*', Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT())),
        File::Spec->devnull()
    );
    my $status_int = Image::LibSIXEL::_sixel_decoder_setopt(
        $decoder,
        ord(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT()),
        File::Spec->devnull()
    );
    Image::LibSIXEL::_sixel_decoder_unref($decoder);
    die 'raw decoder_setopt accepted single-byte bytes option flag input'
        if $status_bytes == Image::LibSIXEL::Constants::SIXEL_OK();
    die 'raw decoder_setopt rejected integer option flag control path'
        if $status_int != Image::LibSIXEL::Constants::SIXEL_OK();
    1;
};

ok($ok, 'raw decoder_setopt rejects single-byte bytes option flag input');
diag($@) if !$ok && $@ ne '';
