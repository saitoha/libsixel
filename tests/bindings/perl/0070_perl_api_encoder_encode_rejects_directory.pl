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
        $encoder->encode(File::Spec->curdir());
        1;
    };
    die 'encoder accepted directory path'
        if $accepted;
    1;
};

ok($ok, 'encoder rejects directory path input');
diag($@) if !$ok && $@ ne '';
