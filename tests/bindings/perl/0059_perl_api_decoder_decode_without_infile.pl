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
    my $source = File::Spec->catfile(
        $ENV{TOP_SRCDIR} // '.',
        qw(tests data inputs snake_64.six)
    );
    my $decoder;
    my $status = Image::LibSIXEL::_sixel_decoder_new(\$decoder, undef);
    die "sixel_decoder_new failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    $status = Image::LibSIXEL::_sixel_decoder_setopt(
        $decoder,
        ord(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_INPUT()),
        $source
    );
    die "sixel_decoder_setopt(input) failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    $status = Image::LibSIXEL::_sixel_decoder_setopt(
        $decoder,
        ord(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT()),
        File::Spec->devnull()
    );
    die "sixel_decoder_setopt(output) failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    $status = Image::LibSIXEL::_sixel_decoder_decode($decoder);
    Image::LibSIXEL::_sixel_decoder_unref($decoder);
    die "decoder decode failed: $status"
        if $status != Image::LibSIXEL::Constants::SIXEL_OK();
    1;
};

ok($ok, 'decoder decode works without infile argument in raw API path');
diag($@) if !$ok && $@ ne '';
