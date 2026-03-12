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

my $ok = eval {
    my $loader = Image::LibSIXEL::sixel_loader_new(undef);
    my $status = Image::LibSIXEL::_sixel_loader_load_file(
        $loader,
        12345,
        0
    );
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader load_file accepted integer filename input'
        if $status == Image::LibSIXEL::Constants::SIXEL_OK();
    1;
};

ok($ok, 'loader load_file rejects integer filename input');
diag($@) if !$ok && $@ ne '';
