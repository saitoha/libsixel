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
    my $output = File::Spec->catfile($artifact_local_dir, 'perl_bindings_smoke.six');

    my $encoder = Image::LibSIXEL::Encoder->new();
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT(), $output);
    $encoder->setopt(Image::LibSIXEL::Constants::SIXEL_OPTFLAG_COLORS(), '16');
    $encoder->encode($source);

    open my $fh, '<', $output or die "failed to read '$output': $!";
    binmode $fh;
    my $payload = do { local $/; <$fh> };
    close $fh;

    die 'encoded output is empty'
        if !defined($payload) || $payload eq '';
    die 'missing sixel DCS introducer'
        if substr($payload, 0, 3) ne "\ePq";
    $payload =~ s/[\r\n]+\z//;
    die 'missing sixel ST terminator'
        if substr($payload, -2) ne "\e\\";
    1;
};

ok($ok, 'encode output generated from packaged perl binding');
diag($@) if !$ok && $@ ne '';
