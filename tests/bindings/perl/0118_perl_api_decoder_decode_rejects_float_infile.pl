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
    $status = Image::LibSIXEL::_sixel_decoder_setopt(
        $decoder,
        ord(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_INPUT()),
        1.5
    );
    Image::LibSIXEL::_sixel_decoder_unref($decoder);
    die 'decoder accepted float infile input path'
        if $status == Image::LibSIXEL::Constants::SIXEL_OK();
    1;
};

ok($ok, 'decoder decode rejects float infile input path');
diag($@) if !$ok && $@ ne '';
