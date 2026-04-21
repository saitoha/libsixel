#!/bin/sh
# TAP test verifying sixel2png help contract remains defined in source.

set -eux

test -f "${TOP_SRCDIR}/converters/sixel2png.c" || {
    printf "1..0 # SKIP missing converters/sixel2png.c\n"
    exit 0
}

echo "1..1"
set -v
set +xv

help_block=''
status=0

help_block=$(cat "${TOP_SRCDIR}/converters/sixel2png.c" 2>/dev/null) || \
    status=$?

test "${status}" -eq 0 || {
    echo "not ok" 1 - "failed to load sixel2png help block from source"
    exit 0
}

test "${help_block#*-H, --help                 show this help.*}" != "${help_block}" || {
    echo "not ok" 1 - "sixel2png source lost -H/--help contract"
    exit 0
}

test "${help_block#*Usage: sixel2png -i input.sixel -o output.png*}" != "${help_block}" || {
    echo "not ok" 1 - "sixel2png source lost usage banner contract"
    exit 0
}

echo "ok" 1 - "sixel2png source keeps help contract"
exit 0
