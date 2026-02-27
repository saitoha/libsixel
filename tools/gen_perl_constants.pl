#!/usr/bin/env perl
use strict;
use warnings;

# Generate Perl constants from include/sixel.h(.in).
#
# Usage:
#   perl tools/gen_perl_constants.pl <input-header> <output-module>

if (@ARGV != 2) {
    die "usage: $0 <input-header> <output-module>\n";
}

my ($in_h, $out_pm) = @ARGV;

open my $in, '<', $in_h or die "cannot open $in_h: $!\n";
my %defs;
my @order;
while (my $line = <$in>) {
    $line =~ s/^\s+|\s+$//g;
    next if $line !~ /^#define\s+/;

    # Parse name/value from '#define NAME value'.
    next if $line !~ /^#define\s+([A-Za-z_][A-Za-z0-9_]*)\s+(.*)$/;
    my ($name, $val) = ($1, $2);

    # Skip function-like macros '#define NAME(arg) ...'.
    next if $name =~ /\($/;
    next if $val eq '';
    next if $name !~ /^(?:SIXEL_|LIBSIXEL_)/;

    # Remove inline comments and surrounding spaces.
    $val =~ s{/\*.*$}{};
    $val =~ s{//.*$}{};
    $val =~ s/^\s+|\s+$//g;
    next if $val eq '';

    if (!exists $defs{$name}) {
        push @order, $name;
    }
    $defs{$name} = $val;
}
close $in;

my %resolved;
for (1 .. 10) {
    my $progress = 0;
    for my $name (@order) {
        next if exists $resolved{$name};
        my $val = $defs{$name};

        if ($val =~ /^"(.*)"$/) {
            my $str = $1;
            $str =~ s/\\/\\\\/g;
            $str =~ s/'/\\'/g;
            $resolved{$name} = "'$str'";
            $progress = 1;
            next;
        }

        if ($val =~ /^'.*'$/) {
            $resolved{$name} = $val;
            $progress = 1;
            next;
        }

        if ($val =~ /^\(?\s*'((?:\\.|[^']))'\s*\)?$/) {
            my $ch = $1;
            $ch =~ s/\\/\\\\/g;
            $ch =~ s/'/\\'/g;
            $resolved{$name} = "'$ch'";
            $progress = 1;
            next;
        }

        my $expr = $val;
        $expr =~ s/([0-9])([uUlL]+)/$1/g;

        for my $sym (@order) {
            next if $expr !~ /\b\Q$sym\E\b/;
            next if !exists $resolved{$sym};
            next if $resolved{$sym} !~ /^(?:0x[0-9A-Fa-f]+|[0-9]+)$/;
            $expr =~ s/\b\Q$sym\E\b/$resolved{$sym}/g;
        }

        next if $expr =~ /@[^@]+@/;
        next if $expr !~ /\A[0-9xXa-fA-F\s\(\)\|\&\^~<>\?\+\-\*\/]+\z/;
        my $out = eval "no warnings; no integer; 0 + ($expr)";
        next if $@;

        $resolved{$name} = int($out);
        $progress = 1;
    }
    last if !$progress;
}

for my $name (@order) {
    if (!exists $resolved{$name}) {
        $resolved{$name} = $defs{$name};
    }
}

open my $out, '>', $out_pm or die "cannot open $out_pm: $!\n";
print {$out} "package Image::LibSIXEL::Constants;\n";
print {$out} "use strict;\n";
print {$out} "use warnings;\n\n";
print {$out} "use Exporter 'import';\n\n";
print {$out} "our \@EXPORT_OK = qw(\n";
for my $name (@order) {
    print {$out} "    $name\n";
}
print {$out} ");\n\n";
print {$out} "# Auto-generated from $in_h. Do not edit manually.\n";
print {$out} "use constant {\n";
for my $name (@order) {
    my $v = $resolved{$name};
    if ($v =~ /^0x[0-9A-Fa-f]+$/ || $v =~ /^[0-9]+$/ || $v =~ /^'.*'$/ || $v =~ /^".*"$/) {
        print {$out} "    $name => $v,\n";
    } else {
        $v =~ s/\\/\\\\/g;
        $v =~ s/'/\\'/g;
        print {$out} "    $name => '$v',\n";
    }
}
print {$out} "};\n\n1;\n";
close $out;
