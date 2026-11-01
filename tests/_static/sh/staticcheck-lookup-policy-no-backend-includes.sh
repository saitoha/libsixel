#!/bin/sh
# Emit TAP for lookup policy include hygiene checks.

set -eu
source_files_helper=$(CDPATH='' cd -- "${0%/*}" && pwd -P)/list-source-files.sh

src_root=$1
src_dir=$src_root/src
matches=

echo "1..1"

if test ! -d "$src_dir"; then
    echo "not ok 1 - lookup policies avoid backend headers"
    echo "# src directory not found: $src_dir"
    exit 1
fi

# shellcheck disable=SC2016
matches=$(sh "$source_files_helper" "$src_dir" --max-depth 1 --name 'lookup-policy-*.c' --null| xargs -0 awk '
$0 ~ /^[[:space:]]*#[[:space:]]*include[[:space:]]+"lookup-8bit\.h"/ {
    print FILENAME ":" FNR ":" $0
    found = 1
}
$0 ~ /^[[:space:]]*#[[:space:]]*include[[:space:]]+"lookup-float32\.h"/ {
    print FILENAME ":" FNR ":" $0
    found = 1
}
END {
    if (found) {
        exit 1
    }
    exit 0
}
' || :)

if test -n "$matches"; then
    echo "not ok 1 - lookup policies avoid backend headers"
    printf '%s\n' "$matches" | sed 's/^/# /'
    exit 1
fi

echo "ok 1 - lookup policies avoid backend headers"
