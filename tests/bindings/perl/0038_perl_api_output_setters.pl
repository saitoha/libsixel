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

if (!eval {
    my $output = Image::LibSIXEL::sixel_output_new(sub { return 0; }, undef, undef);
    Image::LibSIXEL::sixel_output_unref($output);
    1;
}) {
    plan skip_all => "output callback API is unavailable: $@";
}

plan tests => 1;

my $ok = eval {
    my $output = Image::LibSIXEL::sixel_output_new(sub { return 0; }, undef, undef);
    Image::LibSIXEL::sixel_output_set_8bit_availability($output, 1);
    Image::LibSIXEL::sixel_output_set_gri_arg_limit($output, 1);
    Image::LibSIXEL::sixel_output_set_penetrate_multiplexer($output, 1);
    Image::LibSIXEL::sixel_output_set_skip_dcs_envelope($output, 1);
    Image::LibSIXEL::sixel_output_set_skip_header($output, 1);
    Image::LibSIXEL::sixel_output_set_palette_type(
        $output,
        Image::LibSIXEL::Constants::SIXEL_PALETTETYPE_RGB()
    );
    Image::LibSIXEL::sixel_output_set_ormode($output, 1);
    Image::LibSIXEL::sixel_output_set_encode_policy(
        $output,
        Image::LibSIXEL::Constants::SIXEL_ENCODEPOLICY_FAST()
    );
    Image::LibSIXEL::sixel_output_unref($output);
    1;
};

ok($ok, 'output setter APIs accept expected values');
diag($@) if !$ok && $@ ne '';
