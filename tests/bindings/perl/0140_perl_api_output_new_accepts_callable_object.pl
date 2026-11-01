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
        package LocalCallable0143;
        use overload '&{}' => sub { sub { return 0; }; }, fallback => 1;
    }
    my $accepted = eval {
        Image::LibSIXEL::sixel_output_new(
            bless({}, 'LocalCallable0143'),
            undef,
            undef
        );
        1;
    };
    die 'output_new accepted callable-object write_proc unexpectedly'
        if $accepted;
    1;
};

ok($ok, 'output_new callable-object write_proc path is rejected in current perl binding');
diag($@) if !$ok && $@ ne '';
