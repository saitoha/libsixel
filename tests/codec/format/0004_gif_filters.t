#!/bin/sh
# Validate GIF conversion with scaling and background.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"
target_sixel="${ARTIFACT_LOCAL_DIR}/snake-gif.sixel"

run_img2sixel -Lbuiltin! -w105% -h100 -B"#000000000" -rne <"${snake_gif}" >"${target_sixel}" || {
    echo "not ok" 1 "GIF conversion with filters failed"
    exit 0
}

echo "ok" 1 "GIF conversion with filters succeeded"

exit 0
