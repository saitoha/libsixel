#!/bin/sh
# Emit TAP for banning direct getenv() calls in C test sources.

set -eu

src_root=${1:-}

echo "1..1"

if test -z "$src_root"; then
    echo "not ok 1 - C test sources avoid direct getenv() calls"
    echo "# src_root argument is required"
    exit 1
fi

tests_root=$src_root/tests
matches=$(mktemp "${TMPDIR:-/tmp}/libsixel-test-getenv-XXXXXX")
trap 'rm -f "$matches"' EXIT HUP INT TERM

if test ! -d "$tests_root"; then
    echo "not ok 1 - C test sources avoid direct getenv() calls"
    echo "# tests directory not found: $tests_root"
    exit 1
fi

find "$tests_root" -type f \( -name '*.c' -o -name '*.h' \
    -o -name '*.inc.c' \) -exec awk '
BEGIN {
    in_block = 0
}
{
    line = $0
    code = ""
    i = 1
    while (i <= length(line)) {
        two = substr(line, i, 2)
        if (in_block) {
            if (two == "*/") {
                in_block = 0
                i += 2
                continue
            }
            i += 1
            continue
        }
        if (two == "/*") {
            in_block = 1
            i += 2
            continue
        }
        if (two == "//") {
            break
        }
        code = code substr(line, i, 1)
        i += 1
    }
    if (code ~ /(^|[^[:alnum:]_])getenv[[:space:]]*\(/) {
        print FILENAME ":" FNR ":" line
    }
}
' {} + > "$matches"

if test -s "$matches"; then
    echo "not ok 1 - C test sources avoid direct getenv() calls"
    sed 's/^/# direct getenv: /' "$matches"
    exit 1
fi

echo "ok 1 - C test sources avoid direct getenv() calls"
