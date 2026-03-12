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

my @required = qw(
    sixel_loader_new
    sixel_loader_load_file
    sixel_loader_setopt
    sixel_output_new
    sixel_dither_new
    sixel_dither_initialize
    sixel_frame_new
    sixel_set_threads
    sixel_helper_compute_depth
);
my @missing = grep { !Image::LibSIXEL->can($_) } @required;

ok(!@missing, 'module exports expected function symbols');
diag('missing exports: ' . join(', ', @missing)) if @missing;
