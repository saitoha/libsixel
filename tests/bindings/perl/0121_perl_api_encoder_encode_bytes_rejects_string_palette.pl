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
    my $rc = system(
        $^X,
        '-MImage::LibSIXEL',
        '-MImage::LibSIXEL::Constants',
        '-MFile::Spec',
        '-e',
        'my $e;'
        . 'my $s=Image::LibSIXEL::_sixel_encoder_new(\$e,undef);'
        . 'exit 99 if $s!=Image::LibSIXEL::Constants::SIXEL_OK();'
        . '$s=Image::LibSIXEL::_sixel_encoder_setopt($e,ord(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT()),File::Spec->devnull());'
        . 'exit 98 if $s!=Image::LibSIXEL::Constants::SIXEL_OK();'
        . 'my $px=pack("C*",0,1,2,3);'
        . 'my $pp=unpack("J",pack("p",$px));'
        . '$s=Image::LibSIXEL::sixel_encoder_encode_bytes($e,$pp,2,2,Image::LibSIXEL::Constants::SIXEL_PIXELFORMAT_PAL8(),"bad-palette",11);'
        . 'Image::LibSIXEL::_sixel_encoder_unref($e);'
        . 'exit($s==Image::LibSIXEL::Constants::SIXEL_OK()?0:1);'
    );
    die 'encoder encode_bytes unexpectedly succeeded with string palette payload'
        if $rc == 0;
    1;
};

ok($ok, 'encoder encode_bytes string palette input does not succeed');
diag($@) if !$ok && $@ ne '';
