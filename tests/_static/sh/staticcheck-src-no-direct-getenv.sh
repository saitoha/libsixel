#!/bin/sh
# Emit TAP for banning direct getenv() calls in src implementation files.

set -eu

src_root=$1
src_dir=$src_root/src
matches=

echo "1..1"

if test ! -d "$src_dir"; then
    echo "not ok 1 - src files avoid direct getenv() calls"
    echo "# src directory not found: $src_dir"
    exit 1
fi

# shellcheck disable=SC2016
matches=$(find "$src_dir" -maxdepth 1 -type f -name '*.c' \
    ! -name 'compat_stub.c' -print0 | \
    xargs -0 awk '
BEGIN {
    in_block = 0
    found = 0
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
        found = 1
    }
}
END {
    if (found) {
        exit 1
    }
    exit 0
}
' || :)

if test -n "$matches"; then
    echo "not ok 1 - src files avoid direct getenv() calls"
    printf '%s\n' "$matches" | sed 's/^/# /'
    exit 1
fi

echo "ok 1 - src files avoid direct getenv() calls"
