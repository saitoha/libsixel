#!/bin/sh
# Emit TAP for enforcing shell test utility rules.
# Policy: docs/testing/guide.md

set -eu
source_files_helper=$(CDPATH='' cd -- "${0%/*}" && pwd -P)/list-source-files.sh

src_root=$1
tests_root=$src_root/tests

echo "1..1"

if test ! -d "$tests_root"; then
    echo "not ok 1 - shell TAP tests use utilities according to policy"
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

# The directive tests exercise the driver through a shared shell helper.
# Check that entry path as well, including literal shell invocations.
directive_matches=$(cd "$tests_root" &&
    awk '
        /^[ \t]*#/ { next }
        /(^|[ \t=\/])(sh|grep|awk|sed)([ \t]|$)/ ||
        /\$\{[A-Za-z0-9_]+:?-(sh|grep|awk|sed)\}/ {
            print FILENAME ":" FNR ":" $0
        }
    ' _support/tools/tap-directive.sh tools/tap/*.t)

if test -n "$matches$directive_matches"; then
    echo "not ok 1 - shell TAP tests use utilities according to policy"
    printf '%s\n' "$matches" "$directive_matches" | sed '/^$/d; s/^/# /'
    exit 1
fi

echo "ok 1 - shell TAP tests use utilities according to policy"
