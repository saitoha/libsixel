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
    {
        package LocalPathLike0093;
        use overload q{""} => sub { $_[0]->{path}; }, fallback => 1;
    }
    my $pathlike = bless { path => $source }, 'LocalPathLike0093';
    my $encoder = Image::LibSIXEL::Encoder->new();
    $encoder->setopt(
        Image::LibSIXEL::Constants::SIXEL_OPTFLAG_OUTPUT(),
        File::Spec->devnull()
    );
    $encoder->encode($pathlike);
    1;
};

ok($ok, 'encoder encode accepts path-like filename input');
diag($@) if !$ok && $@ ne '';
