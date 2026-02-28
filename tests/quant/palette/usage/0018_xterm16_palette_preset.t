#!/bin/sh
# Re-encode Sixel using xterm16 palette preset.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_six="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
target_sixel="${ARTIFACT_LOCAL_DIR}/sixel-xterm16.sixel"

run_img2sixel -bxterm16 "${snake_six}" >"${target_sixel}" || {
    echo "not ok" 1 "xterm16 preset failed"
    exit 0
}

echo "ok" 1 "xterm16 preset re-encodes Sixel"

exit 0
