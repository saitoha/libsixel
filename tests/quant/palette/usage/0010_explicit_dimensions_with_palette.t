#!/bin/sh
# Check explicit dimensions and palette options work together.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
snake_dims="${ARTIFACT_LOCAL_DIR}/snake-dims.sixel"

run_img2sixel -w210 -h210 -djajuni -bxterm256 -o "${snake_dims}" <"${snake_jpg}" || {
    echo "not ok" 1 - "explicit dimensions and palette options failed"
    exit 0
}

echo "ok" 1 - "explicit dimensions and palette options succeeded"

exit 0
