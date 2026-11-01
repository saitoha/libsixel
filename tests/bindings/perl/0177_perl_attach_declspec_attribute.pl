#!/usr/bin/env perl

use strict;
use warnings;
use Test::More;
use File::Temp qw(tempfile);

plan tests => 1;

my $ok = eval {
    # The output file is required by the generator and is inspected below.
    my ($handle, $output) = tempfile('perl-attach-XXXXXX',
        TMPDIR => 1, UNLINK => 1);
    close $handle or die "cannot close temporary output: $!";
    local @ARGV = (
        "$ENV{TOP_SRCDIR}/tests/_support/bindings/perl-attach-declspec.h",
        $output
    );
    do "$ENV{TOP_SRCDIR}/tools/gen_perl_attach.pl"
        or die $@ || $! || 'attach generator failed';
    open my $generated, '<', $output or die "cannot read output: $!";
    my $text = do { local $/; <$generated> };
    close $generated or die "cannot close generated output: $!";
    die 'attribute changed or hid the generated function signature'
        if index($text,
            q{$ffi->attach(['sixel_probe' => 'Image::LibSIXEL::sixel_probe'] => ['int'] => 'void');}
        ) < 0;
    1;
};

ok($ok, 'declspec declaration attribute preserves the Perl FFI signature');
diag($@) if !$ok && $@ ne '';
