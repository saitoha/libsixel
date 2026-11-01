#!/bin/sh
# Emit TAP for dither policy new-function analyzer guard synchronization.

set -eu
source_files_helper=$(CDPATH='' cd -- "${0%/*}" && pwd -P)/list-source-files.sh

src_root=${1:-}

echo "1..1"

if test -z "$src_root"; then
    echo "not ok 1 - dither policy new analyzer guards stay synchronized"
    echo "# src_root argument is required"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-dither-analyzer-guard-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

bad=$tmpdir/bad.txt
: > "$bad"

sh "$source_files_helper" "$src_root/src" --max-depth 1 --name 'dither-policy-*.c'| LC_ALL=C sort | while IFS= read -r path
do
    create_line=$(awk '
    /sixel_dither_policy_[A-Za-z0-9_]+_new[[:space:]]*\(/ {
        print NR
        exit 0
    }
    ' "$path")
    test -n "$create_line" || continue

    if ! awk -v limit="$create_line" '
    NR < limit && /#if defined\(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK\)/ {
        have_if = 1
    }
    NR < limit && /# pragma GCC diagnostic push/ {
        have_push = 1
    }
    NR < limit && /# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"/ {
        have_ignored = 1
    }
    END {
        exit (have_if && have_push && have_ignored) ? 0 : 1
    }
    ' "$path"; then
        printf '%s:%s:missing analyzer guard before create function\n' \
            "$path" "$create_line" >> "$bad"
    fi

    if ! awk -v start="$create_line" '
    NR > start && /#if defined\(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK\)/ {
        have_if = 1
    }
    NR > start && /# pragma GCC diagnostic pop/ {
        have_pop = 1
    }
    END {
        exit (have_if && have_pop) ? 0 : 1
    }
    ' "$path"; then
        printf '%s:%s:missing analyzer guard terminator after create function\n' \
            "$path" "$create_line" >> "$bad"
    fi
done

if test -s "$bad"; then
    echo "not ok 1 - dither policy new analyzer guards stay synchronized"
    sed 's/^/# /' "$bad"
    exit 1
fi

echo "ok 1 - dither policy new analyzer guards stay synchronized"
exit 0
