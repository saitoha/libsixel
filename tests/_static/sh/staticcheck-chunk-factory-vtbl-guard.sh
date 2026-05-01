#!/bin/sh
# Emit TAP for chunk factory helper vtbl postcondition guards.

set -eu

src_root=${1:-}

echo "1..2"

if test -z "$src_root"; then
    echo "not ok 1 - chunk source factory helper checks init_source vtbl"
    echo "# src_root argument is required"
    echo "not ok 2 - chunk memory factory helper checks init_memory vtbl"
    echo "# src_root argument is required"
    exit 1
fi

header=$src_root/src/chunk-factory.h

if test ! -f "$header"; then
    echo "not ok 1 - chunk source factory helper checks init_source vtbl"
    echo "# missing header: $header"
    echo "not ok 2 - chunk memory factory helper checks init_memory vtbl"
    echo "# missing header: $header"
    exit 1
fi

check_method() {
    helper=$1
    method=$2
    report=$3

    awk -v helper="$helper" -v method="$method" '
    $0 ~ helper "[[:space:]]*\\(" {
        in_helper = 1
        saw_chunk = 0
        saw_vtbl = 0
        saw_method = 0
        saw_unref = 0
        saw_call = 0
    }
    in_helper != 0 {
        if ($0 ~ /chunk[[:space:]]*==[[:space:]]*NULL/) {
            saw_chunk = 1
        }
        if ($0 ~ /chunk->vtbl[[:space:]]*==[[:space:]]*NULL/) {
            saw_vtbl = 1
        }
        if ($0 ~ "chunk->vtbl->" method "[[:space:]]*==[[:space:]]*NULL") {
            saw_method = 1
        }
        if ($0 ~ /chunk->vtbl->unref[[:space:]]*==[[:space:]]*NULL/) {
            saw_unref = 1
        }
        if ($0 ~ "chunk->vtbl->" method "[[:space:]]*\\(") {
            saw_call = 1
            if (!saw_chunk || !saw_vtbl || !saw_method || !saw_unref) {
                printf "%s:%d:%s\n", FILENAME, FNR, $0
                exit 1
            }
            exit 0
        }
    }
    END {
        if (saw_call == 0) {
            exit 1
        }
    }
    ' "$header" > "$report"
}

source_report=$(mktemp "${TMPDIR:-/tmp}/libsixel-chunk-source-guard-XXXXXX")
memory_report=$(mktemp "${TMPDIR:-/tmp}/libsixel-chunk-memory-guard-XXXXXX")
trap 'rm -f "$source_report" "$memory_report"' EXIT HUP INT TERM
failed=0

if check_method "sixel_chunk_create_from_source" "init_source" \
    "$source_report"; then
    echo "ok 1 - chunk source factory helper checks init_source vtbl"
else
    echo "not ok 1 - chunk source factory helper checks init_source vtbl"
    if test -s "$source_report"; then
        sed 's/^/# missing vtbl guard before call: /' "$source_report"
    else
        echo "# init_source call or guard was not found in $header"
    fi
    failed=1
fi

if check_method "sixel_chunk_create_from_memory" "init_memory" \
    "$memory_report"; then
    echo "ok 2 - chunk memory factory helper checks init_memory vtbl"
else
    echo "not ok 2 - chunk memory factory helper checks init_memory vtbl"
    if test -s "$memory_report"; then
        sed 's/^/# missing vtbl guard before call: /' "$memory_report"
    else
        echo "# init_memory call or guard was not found in $header"
    fi
    failed=1
fi

exit "$failed"
