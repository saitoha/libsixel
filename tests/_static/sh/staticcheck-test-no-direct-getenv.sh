#!/bin/sh
# Emit TAP for banning direct CRT-sensitive calls in C test sources.

set -eu

src_root=${1:-}

echo "1..2"

if test -z "$src_root"; then
    echo "not ok 1 - C test sources avoid direct getenv() calls"
    echo "# src_root argument is required"
    echo "not ok 2 - C test sources avoid direct fopen() calls"
    echo "# src_root argument is required"
    exit 1
fi

tests_root=$src_root/tests
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-test-crt-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

getenv_matches=$tmpdir/getenv.txt
fopen_matches=$tmpdir/fopen.txt

if test ! -d "$tests_root"; then
    echo "not ok 1 - C test sources avoid direct getenv() calls"
    echo "# tests directory not found: $tests_root"
    echo "not ok 2 - C test sources avoid direct fopen() calls"
    echo "# tests directory not found: $tests_root"
    exit 1
fi

find "$tests_root" -type f \( -name '*.c' -o -name '*.h' \
    -o -name '*.inc.c' \) -exec awk -v getenv_matches="$getenv_matches" \
    -v fopen_matches="$fopen_matches" '
# Skip complete comment/code spans without rescanning each character.
function strip_comments(line, code, block_start, line_start, block_end) {
    code = ""
    while (line != "") {
        if (in_block) {
            block_end = index(line, "*/")
            if (!block_end) return code
            line = substr(line, block_end + 2)
            in_block = 0
            continue
        }
        block_start = index(line, "/*")
        line_start = index(line, "//")
        if (line_start && (!block_start || line_start < block_start)) {
            return code substr(line, 1, line_start - 1)
        }
        if (!block_start) return code line
        code = code substr(line, 1, block_start - 1)
        line = substr(line, block_start + 2)
        in_block = 1
    }
    return code
}
BEGIN {
    in_block = 0
}
{
    line = $0
    code = strip_comments(line)
    if (code ~ /(^|[^[:alnum:]_])getenv[[:space:]]*\(/) {
        print FILENAME ":" FNR ":" line >> getenv_matches
    }
    if (code ~ /(^|[^[:alnum:]_])fopen[[:space:]]*\(/) {
        print FILENAME ":" FNR ":" line >> fopen_matches
    }
}
' {} +

status=0

if test -s "$getenv_matches"; then
    echo "not ok 1 - C test sources avoid direct getenv() calls"
    sed 's/^/# direct getenv: /' "$getenv_matches"
    status=1
else
    echo "ok 1 - C test sources avoid direct getenv() calls"
fi

if test -s "$fopen_matches"; then
    echo "not ok 2 - C test sources avoid direct fopen() calls"
    sed 's/^/# direct fopen: /' "$fopen_matches"
    status=1
else
    echo "ok 2 - C test sources avoid direct fopen() calls"
fi

exit "$status"
