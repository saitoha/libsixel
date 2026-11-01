#!/bin/sh
# Emit TAP for enforcing grep/awk/sed ban in shell TAP test scripts.
# Policy: docs/testing/guide.md

set -eu
source_files_helper=$(CDPATH='' cd -- "${0%/*}" && pwd -P)/list-source-files.sh

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
    matches=$(sh "$source_files_helper" "$tests_root" --name '*.t' --null| \
        xargs -0 grep -nE \
        '(^|[^[:alnum:]_])(grep|awk|sed)($|[^[:alnum:]_])' || :)
fi

if test -n "$matches"; then
    echo "not ok 1 - shell TAP tests avoid grep/awk/sed"
    printf '%s\n' "$matches" | sed 's/^/# /'
    exit 1
fi

echo "ok 1 - shell TAP tests avoid grep/awk/sed"
