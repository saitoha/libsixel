#!/bin/sh
# TAP test confirming forced libwebp loader rejects chunk payload overruns.

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

stderr_log="${ARTIFACT_LOCAL_DIR}/webp-bad-chunk-payload-overrun.stderr"

run_img2sixel -L libwebp! "${TOP_SRCDIR}/tests/data/corrupted/bad_chunk_payload_exceeds_riff.webp" \
    >/dev/null 2>"${stderr_log}" && {
    echo "not ok" 1 - "forced libwebp loader accepted chunk payload overrun"
    exit 0
}

grep -F "webp decode: chunk payload exceeds RIFF size." "${stderr_log}" >/dev/null || {
    echo "not ok" 1 - "expected chunk payload overrun diagnostic was missing"
    exit 0
}

echo "ok" 1 - "forced libwebp loader rejects chunk payload overrun"

exit 0
