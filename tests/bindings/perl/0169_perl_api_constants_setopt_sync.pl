#!/usr/bin/env perl

use strict;
use warnings;

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

my $header = ($ENV{TOP_SRCDIR} // '.') . '/include/sixel.h.in';
my @expected;
my %seen;
my @missing;
my $line;
my $name;
my $fh;
my $ok_loader;
my $loader;
my $constants_ok;
my $loader_err;
my $ok_all;

open $fh, '<', $header or die "cannot open $header: $!";
while ($line = <$fh>) {
    if ($line =~ /^#define\s+(SIXEL_[A-Z0-9_]+)\s+/) {
        $name = $1;
        next if $name !~ /^SIXEL_(?:LOADER_OPTION|LUT_POLICY|GPU_POLICY|COLORSPACE)_/;
        next if $seen{$name};
        push @expected, $name;
        $seen{$name} = 1;
    }
}
close $fh;

for $name (@expected) {
    if (!Image::LibSIXEL::Constants->can($name)) {
        push @missing, $name;
    }
}

$constants_ok = @missing == 0 &&
    Image::LibSIXEL::Constants::SIXEL_OPTFLAG_SAMPLING_POLICY() == 0x100 &&
    Image::LibSIXEL::Constants::SIXEL_OPTFLAG_BINNING_POLICY() == 0x101 &&
    Image::LibSIXEL::Constants::SIXEL_OPTFLAG_BACKGROUND_POLICY() == 0x102;

$ok_loader = eval {
    $loader = Image::LibSIXEL::sixel_loader_new(undef);
    Image::LibSIXEL::sixel_loader_setopt(
        $loader,
        Image::LibSIXEL::Constants::SIXEL_LOADER_OPTION_START_FRAME_NO(),
        '0'
    );
    Image::LibSIXEL::sixel_loader_unref($loader);
    1;
};
$loader_err = $@;

$ok_all = $constants_ok && $ok_loader;
ok(
    $ok_all,
    'setopt-related constants and loader setopt behavior are synchronized'
);

diag('missing constants: ' . join(', ', @missing)) if !$constants_ok && @missing;
diag('SIXEL_OPTFLAG_SAMPLING_POLICY value mismatch')
    if Image::LibSIXEL::Constants::SIXEL_OPTFLAG_SAMPLING_POLICY() != 0x100;
diag('SIXEL_OPTFLAG_BINNING_POLICY value mismatch')
    if Image::LibSIXEL::Constants::SIXEL_OPTFLAG_BINNING_POLICY() != 0x101;
diag('SIXEL_OPTFLAG_BACKGROUND_POLICY value mismatch')
    if Image::LibSIXEL::Constants::SIXEL_OPTFLAG_BACKGROUND_POLICY() != 0x102;
diag($loader_err) if !$ok_loader && $loader_err ne '';
