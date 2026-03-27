#!/bin/sh
# TAP test confirming forced libwebp loader rejects invalid ANIM chunk size.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

stderr_log="${ARTIFACT_LOCAL_DIR}/webp-bad-anim-size.stderr"

run_img2sixel -L libwebp! "${TOP_SRCDIR}/tests/data/corrupted/bad_anim_chunk_size.webp" \
    >/dev/null 2>"${stderr_log}" && {
    echo "not ok" 1 - "forced libwebp loader accepted invalid ANIM chunk size"
    exit 0
}

grep -F "webp decode: ANIM chunk size is invalid." "${stderr_log}" >/dev/null || {
    echo "not ok" 1 - "expected ANIM chunk size diagnostic was missing"
    exit 0
}

echo "ok" 1 - "forced libwebp loader rejects invalid ANIM chunk size"

exit 0
