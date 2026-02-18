#!/bin/sh
# Confirm prefixed PNG output respects explicit path.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_png="${ARTIFACT_LOCAL_DIR}/snake-explicit.png"

run_img2sixel -o "png:${target_png}" "${snake_jpg}" || {
    fail 1 "prefixed PNG conversion failed"
    exit 0
}

test -s "${target_png}" || {
    fail 1 "prefixed PNG did not produce file"
    exit 0
}

pass 1 "prefixed PNG writes to explicit path"

exit 0
