#!/bin/sh
# TAP test: ACT import accepts transparency index 0 and count 256.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/act-transparency-0-count-256.act"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin -m "${act_palette}" \
    "${snake_png}" -o/dev/null || {
    echo "not ok" 1 - "ACT transparency=0,count=256 should be accepted"
    exit 0
}

echo "ok" 1 - "ACT transparency=0,count=256 is accepted"

exit 0
