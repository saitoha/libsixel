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
        qw(tests data security fuzzing data fuzz0001 libpng_short_signature.bin)
    );
    my $encoder = Image::LibSIXEL::Encoder->new();
    my $failed = 0;
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT(), File::Spec->devnull());
    eval { $encoder->encode($source); 1 } or $failed = 1;
    die 'encoder accepted unsupported input format' if !$failed;
    1;
};

ok($ok, 'encoder rejects unsupported input format');
diag($@) if !$ok && $@ ne '';
