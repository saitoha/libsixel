#!/bin/sh
# Emit TAP for enforcing grep/awk/sed ban in shell TAP test scripts.

set -eu

src_root=$1
tests_root=$src_root/tests

echo "1..1"

if test ! -d "$tests_root"; then
    echo "not ok 1 - shell TAP tests avoid grep/awk/sed"
    echo "# tests directory not found: $tests_root"
    exit 1
fi

if command -v rg >/dev/null 2>&1; then
    matches=$(cd "$src_root" && rg -n --glob 'tests/**/*.t' \
        '(^|[^[:alnum:]_])(grep|awk|sed)($|[^[:alnum:]_])' tests || :)
else
    matches=$(find "$tests_root" -type f -name '*.t' -print0 | \
        xargs -0 grep -nE \
        '(^|[^[:alnum:]_])(grep|awk|sed)($|[^[:alnum:]_])' || :)
fi

if test -n "$matches"; then
    echo "not ok 1 - shell TAP tests avoid grep/awk/sed"
    printf '%s\n' "$matches" | sed 's/^/# /'
    exit 1
fi

echo "ok 1 - shell TAP tests avoid grep/awk/sed"
