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
        qw(tests data inputs snake_64.png)
    );
    my $encoder = Image::LibSIXEL::Encoder->new();
    $encoder->setopt(
        Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT(),
        File::Spec->devnull()
    );
    $encoder->encode(pack('C*', unpack('C*', $source)));
    1;
};

ok($ok, 'encoder encode accepts bytes filename input');
diag($@) if !$ok && $@ ne '';
