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
    my $missing = File::Spec->catfile(
        $ENV{TOP_SRCDIR} // '.',
        qw(tests data inputs definitely-missing-image.png)
    );
    my $failed = 0;
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT(), File::Spec->devnull());
    eval { $encoder->encode($missing); 1 } or $failed = 1;
    die 'encoder accepted missing input path' if !$failed;
    1;
};

ok($ok, 'encoder rejects missing input path');
diag($@) if !$ok && $@ ne '';
