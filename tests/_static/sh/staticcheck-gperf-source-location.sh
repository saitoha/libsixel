#!/bin/sh
# Generated headers must not retain generator-relative source locations.
# Policy: docs/build/autotools.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
awk '
function directive(line) {
    return line ~ /^[[:space:]]*#[[:space:]]*line[[:space:]]/
}
BEGIN {
    if (!directive("#line 4 \"../src/input.gperf\"") ||
            !directive(" # line 4 \"input.gperf\"") ||
            directive("/* #line 4 */")) bad = 1
}
FILENAME ~ /[.]h$/ && directive($0) {
    print "generator-relative source location: " FILENAME ":" FNR
    bad = 1
}
FILENAME ~ /Makefile[.]am$/ {
    if ($0 ~ /^gen-(rgblookup|classid):/) active = 1
    else if ($0 ~ /^[^[:space:]#]/) active = 0
    if (active && $0 ~ /^[[:space:]]*gperf /) pending = 1
    if (active && index($0, "/^#line /d")) {
        if (!pending) bad = 1
        pending = 0
        ++filters
    }
}
END { if (bad || pending || filters != 3) exit 1 }
' "$src_root/src/rgblookup.h" "$src_root/src/classid-factory.h" \
    "$src_root/src/classid-service.h" "$src_root/src/Makefile.am" || {
    echo 'not ok 1 - gperf source locations belong to distributed headers'
    exit 0
}
echo 'ok 1 - gperf source locations belong to distributed headers'
exit 0
