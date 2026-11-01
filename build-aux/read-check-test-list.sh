#!/bin/sh
# Convert metadata into a space-delimited test list.
# Supported mode:
#   tests -> scan the tests source tree and print runnable test scripts

set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 4 ]; then
    echo "Usage: $0 <tests> <tests-dir> [include|skip] [space|newline|make]" >&2
    exit 1
fi

mode=$1
tests_dir=$2
ruby_tests_mode=${3:-include}
output_mode=${4:-space}

case "$ruby_tests_mode" in
    include|skip)
        ;;
    *)
        echo "Unknown ruby-tests mode: $ruby_tests_mode" >&2
        exit 1
        ;;
esac

case "$output_mode" in
    space|newline|make)
        ;;
    *)
        echo "Unknown output mode: $output_mode" >&2
        exit 1
        ;;
esac

if [ ! -d "$tests_dir" ]; then
    echo "Tests directory not found: $tests_dir" >&2
    exit 1
fi

case "$mode" in
    tests)
        ;;
    *)
        echo "Unknown mode: $mode" >&2
        exit 1
        ;;
esac

# Run the scan from tests_dir and emit normalized relative paths.
# This avoids platform-specific absolute path forms (for example C:/...)
# from leaking into Meson test names where ':' is deprecated.
# POSIX shells lack pipefail.  Pass scan/sort failures through the stream so
# the final awk cannot turn a partial inventory into a successful result.
{
    (
        cd "$tests_dir" || exit 1
        # Category depth is enforced by staticcheck. One bulk glob avoids
        # a process or metadata probe per case and needs no findutils.
        LC_ALL=C
        export LC_ALL
        printf '%s\n' ./*/*/[0-9][0-9][0-9][0-9]_*.t \
            ./bindings/php/[0-9][0-9][0-9][0-9]_*.php \
            ./bindings/python/[0-9][0-9][0-9][0-9]_*.py \
            ./bindings/ruby/[0-9][0-9][0-9][0-9]_*.rb \
            ./bindings/perl/[0-9][0-9][0-9][0-9]_*.pl
    ) || printf '%s\n' __SIXEL_TEST_DISCOVERY_FAILED__
} |
{
    LC_ALL=C sort || printf '%s\n' __SIXEL_TEST_DISCOVERY_FAILED__
} |
    awk -v ruby_tests_mode="$ruby_tests_mode" \
        -v output_mode="$output_mode" '
        function emit_make_path(path) {
            prefix = "TESTS +="
            extra = length(path) + 1
            if (make_line_len == 0) {
                printf "%s %s", prefix, path
                make_line_len = length(prefix) + extra
            } else if (make_line_len + extra > 1800) {
                printf "\n%s %s", prefix, path
                make_line_len = length(prefix) + extra
            } else {
                printf " %s", path
                make_line_len += extra
            }
        }
        BEGIN {
            if (output_mode == "make") {
                printf "TESTS =\n"
            }
        }
        $0 == "__SIXEL_TEST_DISCOVERY_FAILED__" {
            failed = 1
            next
        }
        {
            path = $0
            sub("^\\./", "", path)
            # An unmatched POSIX glob remains literal. Case paths cannot
            # contain wildcard characters under the naming contract.
            if (index(path, "*") != 0) {
                next
            }
            if (ruby_tests_mode == "skip" &&
                path ~ /^bindings\/ruby\/[0-9][0-9][0-9][0-9]_.+\.rb$/) {
                next
            }
            if (output_mode == "make") {
                emit_make_path(path)
            } else if (output_mode == "newline") {
                printf "%s\n", path
            } else {
                printf "%s ", path
            }
        }
        END {
            if (failed) {
                exit 1
            }
            if (output_mode == "make" && make_line_len != 0) {
                printf "\n"
            } else if (output_mode != "newline") {
                printf "\n"
            }
        }
    '
