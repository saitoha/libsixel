#!/bin/sh
# TAP test verifying -Q accepts scene_cut_threshold on medoids model.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qmedoids:scene_cut_threshold=0.20 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o /dev/null >/dev/null 2>&1 || {
    echo "not ok" 1 - "-Q medoids:scene_cut_threshold=0.20 was rejected"
    exit 0
}

echo "ok" 1 - "-Q accepts scene_cut_threshold on medoids model"
exit 0
