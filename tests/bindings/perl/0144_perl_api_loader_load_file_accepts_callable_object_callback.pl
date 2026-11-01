#!/usr/bin/env perl

use strict;
use warnings;

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
    {
        package LocalCallable0147;
        use overload '&{}' => sub { sub { return 0; }; }, fallback => 1;
    }
    my $loader = Image::LibSIXEL::sixel_loader_new(undef);
    my $accepted = eval {
        Image::LibSIXEL::sixel_loader_load_file(
            $loader,
            'dummy.png',
            bless({}, 'LocalCallable0147')
        );
        1;
    };
    Image::LibSIXEL::sixel_loader_unref($loader);
    die 'loader load_file accepted callable-object callback unexpectedly'
        if $accepted;
    1;
};

ok($ok, 'loader callable-object callback path is rejected in current perl binding');
diag($@) if !$ok && $@ ne '';
