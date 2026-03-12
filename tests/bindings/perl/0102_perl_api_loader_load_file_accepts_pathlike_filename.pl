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
        package LocalPathLike0102;
        use overload q{""} => sub { $_[0]->{path}; }, fallback => 1;
    }
    my $pathlike = bless { path => $source }, 'LocalPathLike0102';
    my $loader = Image::LibSIXEL::sixel_loader_new(undef);
    my $status_pathlike = Image::LibSIXEL::_sixel_loader_load_file(
        $loader,
        $pathlike,
        0
    );
    my $status_string = Image::LibSIXEL::_sixel_loader_load_file(
        $loader,
        $source,
        0
    );
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader path-like filename path diverged from string path'
        if $status_pathlike != $status_string;
    die 'loader path-like filename unexpectedly succeeded with null callback'
        if $status_pathlike == Image::LibSIXEL::Constants::SIXEL_OK();
    1;
};

ok($ok, 'loader load_file path-like filename coercion matches string path');
diag($@) if !$ok && $@ ne '';
