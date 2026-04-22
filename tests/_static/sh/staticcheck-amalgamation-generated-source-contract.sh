#!/bin/sh
# Emit TAP for generated amalgamation source contract checks.

set -eu

echo "1..1"

src_root=$1
build_root=${2:-${TOP_BUILDDIR:-$src_root}}
generator=$src_root/tools/gen-amalgamation.sh

if test ! -f "$generator"; then
    echo "ok 1 # SKIP missing tools/gen-amalgamation.sh"
    exit 0
fi

if test ! -f "$build_root/config.h"; then
    echo "ok 1 # SKIP missing config.h in build root"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-staticcheck-amalgamation-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

generated=$tmpdir/sixel.c
generator_log=$tmpdir/generator.log
violations=$tmpdir/violations.log

if ! "$generator" "$generated" "$src_root" "$build_root" \
    >"$generator_log" 2>&1; then
    echo "not ok 1 - amalgamation source keeps tool-safe include contract"
    echo "# failed to generate amalgamation source"
    sed 's/^/# /' "$generator_log"
    exit 1
fi

if awk '
/^\/\* ==== tests\// {
    printf "# line %d: unexpected test unit marker: %s\n", NR, $0
    bad = 1
}
/^[[:space:]]*#[[:space:]]*include[[:space:]]*["<](src|tests)\// {
    printf "# line %d: unexpected private include: %s\n", NR, $0
    bad = 1
}
END {
    exit bad ? 1 : 0
}
' "$generated" >"$violations"; then
    echo "ok 1 - amalgamation source keeps tool-safe include contract"
    exit 0
fi

echo "not ok 1 - amalgamation source keeps tool-safe include contract"
cat "$violations"
exit 1
