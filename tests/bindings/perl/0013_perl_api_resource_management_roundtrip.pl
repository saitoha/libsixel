#!/usr/bin/env perl

use strict;
use warnings;

use File::Path qw(make_path);
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

my $artifact_local_dir = $ENV{ARTIFACT_LOCAL_DIR}
    // File::Spec->catdir('.', 'tests', '_artifacts', 'perl');
make_path($artifact_local_dir);

my $ok = eval {
    my $source = File::Spec->catfile(
        $ENV{TOP_SRCDIR} // '.',
        qw(tests data inputs snake_64.png)
    );
    my $sixel_path = File::Spec->catfile($artifact_local_dir, 'resource_roundtrip.six');
    my $png_path = File::Spec->catfile($artifact_local_dir, 'resource_roundtrip.png');

    my $encoder = Image::LibSIXEL::Encoder->new();
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT(), $sixel_path);
    $encoder->encode($source);

    my $decoder = Image::LibSIXEL::Decoder->new();
    $decoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_INPUT(), $sixel_path);
    $decoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT(), $png_path);
    $decoder->decode();

    die "roundtrip png output is missing or empty"
        if !-f $png_path || -z $png_path;
    1;
};

ok($ok, 'encoder/decoder roundtrip creates png output');
diag($@) if !$ok && $@ ne '';
