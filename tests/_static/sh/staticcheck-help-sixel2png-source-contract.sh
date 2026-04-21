#!/bin/sh
# Emit TAP for sixel2png source help contracts in converters/sixel2png.c.

set -eu

echo "1..1"

src_root=$1
source_file=$src_root/converters/sixel2png.c
help_block=''
status=0

if test ! -f "$source_file"; then
    echo "ok 1 # SKIP missing converters/sixel2png.c"
    exit 0
fi

help_block=$(cat "$source_file" 2>/dev/null) || status=$?

test "$status" -eq 0 || {
    echo "not ok 1 - failed to read converters/sixel2png.c"
    exit 0
}

test "${help_block#*-H, --help                 show this help.*}" \
    != "${help_block}" || {
    echo "not ok 1 - sixel2png source lost -H/--help contract"
    exit 0
}

test "${help_block#*Usage: sixel2png -i input.sixel -o output.png*}" \
    != "${help_block}" || {
    echo "not ok 1 - sixel2png source lost usage banner contract"
    exit 0
}

echo "ok 1 - sixel2png source keeps help contract"
exit 0
