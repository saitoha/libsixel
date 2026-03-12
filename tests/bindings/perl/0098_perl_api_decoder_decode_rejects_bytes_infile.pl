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
    my $decoder = Image::LibSIXEL::Decoder->new();
    $decoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_INPUT(), $source);
    $decoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT(), File::Spec->devnull());
    $decoder->decode(pack('C*', unpack('C*', '/path/which/does/not/exist.six')));
    1;
};

ok($ok, 'decoder decode rejects bytes infile override and uses configured input');
diag($@) if !$ok && $@ ne '';
