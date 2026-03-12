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
    my $encoder = Image::LibSIXEL::Encoder->new();
    $encoder->setopt(
        Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT(),
        File::Spec->devnull()
    );
    my $accepted = eval {
        $encoder->encode(pack('C*', unpack('C*', '.')));
        1;
    };
    die 'encoder accepted directory bytes filename input'
        if $accepted;
    1;
};

ok($ok, 'encoder encode rejects directory bytes filename input');
diag($@) if !$ok && $@ ne '';
