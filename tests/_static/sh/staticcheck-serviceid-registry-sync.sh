#!/bin/sh
# Emit TAP for service-id annotation and gperf sync checks.

set -eu

src_root=$1
generator=$src_root/tools/gen_serviceid_gperf.awk
gperf_file=$src_root/src/classid-service.gperf

echo "1..1"

if test ! -f "$generator"; then
    echo "not ok 1 - components serviceid registry stays in sync"
    echo "# missing generator: $generator"
    exit 1
fi

if test ! -f "$gperf_file"; then
    echo "not ok 1 - components serviceid registry stays in sync"
    echo "# missing gperf file: $gperf_file"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-serviceid-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected=$tmpdir/classid-service.expected.gperf

if ! awk -f "$generator" \
    "$src_root"/src/factory.h \
    >"$expected"; then
    echo "not ok 1 - components serviceid registry stays in sync"
    echo "# failed to regenerate classid-service.gperf"
    exit 1
fi

if cmp -s "$gperf_file" "$expected"; then
    echo "ok 1 - components serviceid registry stays in sync"
    exit 0
fi

echo "not ok 1 - components serviceid registry stays in sync"
if command -v diff >/dev/null 2>&1; then
    diff -u "$gperf_file" "$expected" | sed 's/^/# /'
else
    echo "# diff not found"
fi
exit 1
