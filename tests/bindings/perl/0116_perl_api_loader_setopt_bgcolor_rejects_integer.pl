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
        '-e',
        'my $l=Image::LibSIXEL::sixel_loader_new(undef);'
        . 'my $s=Image::LibSIXEL::_sixel_loader_setopt($l,Image::LibSIXEL::Constants::SIXEL_LOADER_OPTION_BGCOLOR(),1);'
        . 'Image::LibSIXEL::sixel_loader_unref($l);'
        . 'exit($s==Image::LibSIXEL::Constants::SIXEL_OK()?0:1);'
    );
    die 'loader setopt unexpectedly succeeded on integer bgcolor input path'
        if $rc == 0;
    1;
};

ok($ok, 'loader setopt bgcolor integer input does not succeed');
diag($@) if !$ok && $@ ne '';
