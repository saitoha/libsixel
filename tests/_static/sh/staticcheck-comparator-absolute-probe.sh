#!/bin/sh
# A build-side comparator must understand the shell's absolute source paths.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function valid(text) {
    return index(text, "sixel_compare_source=`cd \"$srcdir\" && pwd`/tools/compare.c") &&
        text ~ /AS_IF\(\[\.\/conftest-compare[^\n]*\\\n[[:space:]]*"\$sixel_compare_source" conftest-compare.c &&/
}
{ text = text $0 "\n" }
FILENAME ~ /lso-tap-driver/ {
    if (/if test.*@sixel_have_compare@.*= yes/) enabled = 1
    if (/^fi$/) enabled = 0
    if (/PATH=.*tools\/compare/ && !enabled) failed = 1
    if (/^PATH=.*build-aux\/compare/) fallback = 1
}
END {
    relative = text
    gsub(/"\$sixel_compare_source"/, "\"$srcdir/tools/compare.c\"", relative)
    exit !valid(text) || valid(relative) || failed || !fallback
}
' "$1/m4/sixel_compare.m4" "$1/build-aux/lso-tap-driver.sh.in" || {
    echo 'not ok 1 - comparator probe includes absolute shell paths'
    exit 0
}
echo 'ok 1 - comparator probe includes absolute shell paths'
