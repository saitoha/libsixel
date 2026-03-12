#!/bin/sh
# Convert metadata into a space-delimited test list.
# Supported mode:
#   tests -> scan the tests source tree and print runnable test scripts

set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "Usage: $0 <tests> <tests-dir> [include|skip]" >&2
    exit 1
fi

mode=$1
tests_dir=$2
ruby_tests_mode=${3:-include}

case "$ruby_tests_mode" in
    include|skip)
        ;;
    *)
        echo "Unknown ruby-tests mode: $ruby_tests_mode" >&2
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
(
    cd "$tests_dir"
    find . \
        \( \
            -path './.perl-test-venv' -o \
            -path './.python-test-venv' -o \
            -path './.ruby-test-venv' -o \
            -path './_artifacts' \
        \) -prune -o \
        -type f \( \
        -name '*.t' -o \
        -path './bindings/python/[0-9][0-9][0-9][0-9]_*.py' -o \
        -path './bindings/ruby/[0-9][0-9][0-9][0-9]_*.rb' -o \
        -path './bindings/perl/[0-9][0-9][0-9][0-9]_*.pl' \
        \) -print
) |
    LC_ALL=C sort |
    awk -v ruby_tests_mode="$ruby_tests_mode" '
        {
            path = $0
            sub("^\\./", "", path)
            if (ruby_tests_mode == "skip" &&
                path ~ /^bindings\/ruby\/[0-9][0-9][0-9][0-9]_.+\.rb$/) {
                next
            }
            printf "%s ", path
        }
        END {
            printf "\n"
        }
    '
