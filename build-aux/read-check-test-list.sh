#!/bin/sh
# Convert metadata into a space-delimited test list.
# Supported mode:
#   tests -> scan the tests source tree and print runnable test scripts

set -eu

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <tests> <tests-dir>" >&2
    exit 1
fi

mode=$1
tests_dir=$2

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

find "$tests_dir" -type f \( \
    -name '*.t' -o \
    -path '*/bindings/python/[0-9][0-9][0-9][0-9]_*.py' \
\) -print |
    LC_ALL=C sort |
    awk -v prefix="$tests_dir/" '
        {
            path = $0
            sub("^" prefix, "", path)
            printf "%s ", path
        }
        END {
            printf "\n"
        }
    '
