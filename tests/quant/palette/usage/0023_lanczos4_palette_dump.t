#!/bin/sh
# Scale with Lanczos4 filter and emit palette dump.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/lanczos4-palette-dump.sixel"

run_img2sixel -e -h140 -rlanczos4 -P "${snake_jpg}" \
        >"${target_sixel}" || {
    fail 1 "Lanczos4 scaling palette dump fails"
    exit 0
}

pass 1 "Lanczos4 scaling emits palette dump"

exit 0
