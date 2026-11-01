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
        $encoder->encode(bless({}, 'LocalNonPathLike0136'));
        1;
    };
    die 'encoder accepted non-pathlike filename object'
        if $accepted;
    1;
};

ok($ok, 'encoder encode rejects non-pathlike filename object');
diag($@) if !$ok && $@ ne '';
