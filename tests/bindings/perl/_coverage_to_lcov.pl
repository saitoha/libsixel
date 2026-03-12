#!/usr/bin/env perl

use strict;
use warnings;

use Cwd qw(abs_path);
use Devel::Cover::DB;
use File::Spec;
use Scalar::Util qw(reftype);

if (@ARGV < 2 || @ARGV > 4) {
    warn "usage: $0 <cover-db-dir> <output-lcov> [source-root] [venv-root]\n";
    exit 2;
}

my $cover_db_dir = $ARGV[0];
my $output_lcov = $ARGV[1];
my $source_root = defined($ARGV[2]) ? abs_path($ARGV[2]) : undef;
my $venv_root = defined($ARGV[3]) ? abs_path($ARGV[3]) : undef;
my $source_perl_lib = defined($source_root)
    ? File::Spec->catdir($source_root, 'perl', 'lib')
    : undef;

my %records;
my $db = Devel::Cover::DB->new(db => $cover_db_dir);
my $cover = $db->cover;

for my $file ($cover->items) {
    my $normalized = $file;
    if (defined($source_root) && $normalized =~ m{(Image/LibSIXEL(?:/[^/]+)*\.pm)\z}) {
        my $suffix = $1;
        my @suffix_parts = split m{/}, $suffix;
        my $candidate = File::Spec->catfile($source_root, 'perl', 'lib', @suffix_parts);
        if (-f $candidate) {
            $normalized = $candidate;
        }
    }

    if (defined($venv_root)) {
        my $venv_lib = File::Spec->catdir($venv_root, 'lib', 'perl5');
        if ($normalized =~ /^\Q$venv_lib\E\/(?:[^\/]+\/)?(Image\/LibSIXEL(?:\/[^\/]+)*\.pm)\z/) {
            my @suffix_parts = split m{/}, $1;
            my $candidate = File::Spec->catfile($source_root, 'perl', 'lib', @suffix_parts);
            if (-f $candidate) {
                $normalized = $candidate;
            }
        }
    }

    if (defined($source_perl_lib) &&
        index($normalized, "$source_perl_lib/") != 0) {
        next;
    }

    my $file_obj = $cover->file($file);
    next if !defined($file_obj);

    my $stmt_criterion;
    for my $criterion_name ($file_obj->items) {
        if ($criterion_name eq 'statement') {
            $stmt_criterion = $file_obj->criterion($criterion_name);
            last;
        }
    }
    next if !defined($stmt_criterion);

    for my $line ($stmt_criterion->items) {
        my $location = $stmt_criterion->location($line);
        next if ref($location) ne 'ARRAY';
        next if !defined($location->[0]) || reftype($location->[0]) ne 'ARRAY';
        my $hit = $location->[0]->[0];
        $hit = 0 if !defined($hit) || $hit < 0;
        $records{$normalized}->{found}->{$line} = 1;
        if ($hit > 0) {
            $records{$normalized}->{hits}->{$line} += $hit;
        }
    }
}

open my $out, '>', $output_lcov
    or die "failed to open '$output_lcov' for writing: $!";

for my $file (sort keys %records) {
    my @lines = sort { $a <=> $b } keys %{ $records{$file}->{found} };
    next if !@lines;

    my $lines_found = 0;
    my $lines_hit = 0;
    print {$out} "TN:\n";
    print {$out} "SF:$file\n";
    for my $line (@lines) {
        my $hit = $records{$file}->{hits}->{$line} // 0;
        print {$out} "DA:$line,$hit\n";
        $lines_found++;
        $lines_hit++ if $hit > 0;
    }
    print {$out} "LF:$lines_found\n";
    print {$out} "LH:$lines_hit\n";
    print {$out} "end_of_record\n";
}

close $out or die "failed to close '$output_lcov': $!";
