#!/bin/sh
# Emit TAP for banning direct CRT-sensitive calls in C test sources.
# Policy: docs/misc/platforms/windows-paths.md
# Coverage: WPATH-07

set -eu

src_root=${1:-}

echo "1..2"

if test -z "$src_root"; then
    echo "not ok 1 - C test sources avoid direct getenv() calls"
    echo "# src_root argument is required"
    echo "not ok 2 - C test streams stay in the test-runner CRT"
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
    echo "not ok 2 - C test streams stay in the test-runner CRT"
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
' {} + >"$getenv_matches"

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
    is_runner_helper = FILENAME ~ /\/tests\/test_runner_io\.h$/
    is_helper_fopen = code ~ /^[[:space:]]*stream = fopen\(path, mode\);[[:space:]]*$/
    if (code ~ /(^|[^[:alnum:]_])fopen[[:space:]]*\(/ &&
        !(is_runner_helper && is_helper_fopen)) {
        print FILENAME ":" FNR ":" line
    }
    if (code ~ /(^|[^[:alnum:]_])sixel_compat_fopen[[:space:]]*\(/) {
        print FILENAME ":" FNR ":" line
    }
}
' {} + >"$fopen_matches"

status=0

if test -s "$getenv_matches"; then
    echo "not ok 1 - C test sources avoid direct getenv() calls"
    sed 's/^/# direct getenv: /' "$getenv_matches"
    status=1
else
    echo "ok 1 - C test sources avoid direct getenv() calls"
fi

if test -s "$fopen_matches"; then
    echo "not ok 2 - C test streams stay in the test-runner CRT"
    sed 's/^/# non-runner fopen: /' "$fopen_matches"
    status=1
else
    echo "ok 2 - C test streams stay in the test-runner CRT"
fi

exit "$status"
