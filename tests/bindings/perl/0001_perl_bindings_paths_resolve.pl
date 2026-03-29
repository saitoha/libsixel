#!/usr/bin/env perl

use strict;
use warnings;

use File::Basename qw(dirname);
use File::Spec;
use Test::More;

my $loaded = eval {
    require Image::LibSIXEL;
    1;
};
if (!$loaded) {
    plan skip_all => "libsixel perl binding failed to load: $@";
}

plan tests => 1;

my $ok = eval {
    my $module_path = $INC{'Image/LibSIXEL.pm'};
    die 'failed to resolve Image/LibSIXEL.pm' if !defined $module_path;

    my $module_root = dirname($module_path);
    my $libs_dir = File::Spec->catdir($module_root, 'LibSIXEL');
    die "bundled library directory is missing: $libs_dir" if !-d $libs_dir;

    my @candidates = (
        File::Spec->catfile($libs_dir, 'libsixel.so.1'),
        File::Spec->catfile($libs_dir, 'libsixel.so'),
        File::Spec->catfile($libs_dir, 'libsixel.dylib'),
        File::Spec->catfile($libs_dir, 'libsixel.dll'),
    );
    my ($found) = grep { -f $_ } @candidates;
    die 'bundled shared library is missing' if !defined $found;
    1;
};

ok($ok, 'packaged perl binding paths are configured');
diag($@) if !$ok && $@ ne '';
