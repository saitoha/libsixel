#!/bin/sh
# Discover runtime scripts and emit a normalized test inventory.
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
        # Category depth is enforced by staticcheck. Globbing needs no
        # per-case metadata probe or findutils. OpenBSD sh runs printf as
        # an external command, so bound each argv even for a large suite.
        LC_ALL=C
        export LC_ALL
        # A shell function accepts the full glob without exec. Shifting a
        # fixed batch avoids rebuilding positional arguments for every path.
        emit_paths() {
            while test "$#" -ge 32; do
                printf '%s\n' \
                    "${1}" "${2}" "${3}" "${4}" "${5}" "${6}" "${7}" "${8}" \
                    "${9}" "${10}" "${11}" "${12}" "${13}" "${14}" "${15}" "${16}" \
                    "${17}" "${18}" "${19}" "${20}" "${21}" "${22}" "${23}" "${24}" \
                    "${25}" "${26}" "${27}" "${28}" "${29}" "${30}" "${31}" "${32}" || return 1
                shift 32
            done
            if test "$#" -ne 0; then
                printf '%s\n' "$@" || return 1
            fi
        }
        emit_paths ./*/*/[0-9][0-9][0-9][0-9]_*.t \
            ./bindings/php/[0-9][0-9][0-9][0-9]_*.php \
            ./bindings/python/[0-9][0-9][0-9][0-9]_*.py \
            ./bindings/ruby/[0-9][0-9][0-9][0-9]_*.rb \
            ./bindings/perl/[0-9][0-9][0-9][0-9]_*.pl \
            ./build/install/[0-9][0-9][0-9][0-9]_*.py \
            ./tools/child-time/[0-9][0-9][0-9][0-9]_*.py
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
