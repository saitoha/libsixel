#!/bin/sh
# TAP test confirming forced libwebp loader rejects invalid WEBP signatures.

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

stderr_log="${ARTIFACT_LOCAL_DIR}/webp-bad-signature.stderr"

run_img2sixel -L libwebp! "${TOP_SRCDIR}/tests/data/corrupted/bad_riff_webp_signature.webp" \
    >/dev/null 2>"${stderr_log}" && {
    echo "not ok" 1 - "forced libwebp loader accepted invalid WEBP signature"
    exit 0
}

grep -F "webp decode: RIFF/WEBP signature is invalid." "${stderr_log}" >/dev/null || {
    echo "not ok" 1 - "expected invalid RIFF/WEBP signature diagnostic was missing"
    exit 0
}

echo "ok" 1 - "forced libwebp loader rejects invalid WEBP signature"

exit 0
