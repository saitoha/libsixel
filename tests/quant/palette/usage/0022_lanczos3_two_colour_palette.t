#!/bin/sh
# Scale with Lanczos3 filter using a two-colour palette.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/lanczos3-two-colour.sixel"

run_img2sixel -p 2 -h100 -wauto -rlanczos3 "${snake_jpg}" \
        >"${target_sixel}" || {
    echo "not ok" 1 - "Lanczos3 scaling with two-colour palette fails"
    exit 0
}

echo "ok" 1 - "Lanczos3 scaling with two-colour palette works"

exit 0
