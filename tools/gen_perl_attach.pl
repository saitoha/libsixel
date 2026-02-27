#!/usr/bin/env perl
use strict;
use warnings;

# Generate Perl FFI attach table from include/sixel.h(.in).
#
# Usage:
#   perl tools/gen_perl_attach.pl <input-header> <output-module>

if (@ARGV != 2) {
    die "usage: $0 <input-header> <output-module>\n";
}

my ($in_h, $out_pm) = @ARGV;
open my $in, '<', $in_h or die "cannot open $in_h: $!\n";
my $text = do { local $/; <$in> };
close $in;

# Remove comments and configure placeholder attributes to simplify parsing.
$text =~ s{/\*.*?\*/}{}gs;
$text =~ s{//.*$}{}mg;
$text =~ s/\@attr_[^@]+\@//g;

my %callback_types = (
    sixel_write_function      => 'sixel_write_callback_t',
    sixel_load_image_function => 'sixel_loader_callback_t',
);

my %callback_signatures = (
    sixel_write_callback_t  => '(opaque,int,opaque)->int',
    sixel_loader_callback_t => '(opaque,opaque)->int',
);

my %alias_override = (
    sixel_helper_format_error => '_sixel_helper_format_error',
    sixel_encoder_new         => '_sixel_encoder_new',
    sixel_encoder_setopt      => '_sixel_encoder_setopt',
    sixel_encoder_encode      => '_sixel_encoder_encode',
    sixel_encoder_unref       => '_sixel_encoder_unref',
    sixel_decoder_new         => '_sixel_decoder_new',
    sixel_decoder_setopt      => '_sixel_decoder_setopt',
    sixel_decoder_decode      => '_sixel_decoder_decode',
    sixel_decoder_unref       => '_sixel_decoder_unref',
    sixel_loader_new          => '_sixel_loader_new',
    sixel_loader_load_file    => '_sixel_loader_load_file',
    sixel_loader_setopt       => '_sixel_loader_setopt',
    sixel_output_new          => '_sixel_output_new',
    sixel_dither_new          => '_sixel_dither_new',
    sixel_dither_initialize   => '_sixel_dither_initialize',
    sixel_frame_new           => '_sixel_frame_new',
);

sub normalize_type {
    my ($raw) = @_;

    $raw =~ s/\bconst\b//g;
    $raw =~ s/\bSIXELAPI\b//g;
    $raw =~ s/^\s+|\s+$//g;
    $raw =~ s/\s+/ /g;
    $raw =~ s/\s*\*\s*/*/g;
    return $raw;
}

sub map_type {
    my ($ctype) = @_;
    my $t = normalize_type($ctype);

    if (exists $callback_types{$t}) {
        return $callback_types{$t};
    }

    return 'void' if $t eq 'void';
    return 'string' if $t eq 'char*';
    return 'int' if $t =~ /^(?:SIXELSTATUS|int|unsigned int|size_t)$/;
    return 'opaque*' if $t =~ /\*\*$/;
    return 'opaque' if $t =~ /\*$/;
    return 'opaque' if $t =~ /_t$/;
    return 'int';
}

sub split_args {
    my ($args_raw) = @_;
    my @args;

    $args_raw =~ s/^\s+|\s+$//g;
    return () if $args_raw eq '' || $args_raw eq 'void';

    for my $arg (split /,/, $args_raw) {
        $arg =~ s/^\s+|\s+$//g;
        next if $arg eq '';
        push @args, $arg;
    }

    return @args;
}

sub drop_param_name {
    my ($arg) = @_;
    my $prefix;

    $arg =~ s/^\s+|\s+$//g;
    return $arg if $arg eq '...';
    if ($arg =~ /^(.*?)([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*$/) {
        $prefix = $1;
        if ($prefix =~ /[\s\*]$/) {
            $arg = $prefix;
        }
    }
    $arg =~ s/^\s+|\s+$//g;
    return $arg;
}

my @functions;
while ($text =~ /SIXELAPI\s+([^;\(]+?)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\((.*?)\)\s*;/sg) {
    my ($ret, $name, $args_raw) = ($1, $2, $3);
    next if $name !~ /^sixel_/;

    my @arg_types;
    for my $arg (split_args($args_raw)) {
        push @arg_types, map_type(drop_param_name($arg));
    }

    push @functions, {
        name => $name,
        perl_name => ($alias_override{$name} // $name),
        ret => map_type($ret),
        args => \@arg_types,
    };
}

open my $out, '>', $out_pm or die "cannot open $out_pm: $!\n";
print {$out} "package Image::LibSIXEL::GeneratedAttach;\n";
print {$out} "use strict;\n";
print {$out} "use warnings;\n\n";
print {$out} "sub attach_all {\n";
print {$out} "    my (\$ffi) = \@_;\n\n";
print {$out} "    # Callback typedefs are mapped to explicit Platypus signatures.\n";
for my $cb (sort keys %callback_signatures) {
    print {$out} "    \$ffi->type('" . $callback_signatures{$cb} . "' => '$cb');\n";
}
print {$out} "\n";
for my $fn (@functions) {
    my $args = join(', ', map { "'$_'" } @{$fn->{args}});
    my $full_name;

    $args = "[$args]";
    $full_name = $fn->{perl_name};
    if ($full_name !~ /::/) {
        $full_name = 'Image::LibSIXEL::' . $full_name;
    }
    print {$out}
        "    \$ffi->attach(['$fn->{name}' => '$full_name'] => $args => '$fn->{ret}');\n";
}
print {$out} "\n    return;\n}\n\n1;\n";
close $out;
