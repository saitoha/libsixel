#!/bin/sh
# TAP test verifying --cms-engine matching is case insensitive.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64_embedded_a98_icc.webp"
output_lower="${ARTIFACT_LOCAL_DIR}/cms_engine_case_ref_auto.six"
output_mixed="${ARTIFACT_LOCAL_DIR}/cms_engine_case_mixed_auto.six"

run_img2sixel \
    --cms-engine=auto \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_lower}" || {
    echo "not ok" 1 - "--cms-engine=auto reference decode failed"
    exit 0
}

run_img2sixel \
    --cms-engine=AuTo \
    -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_mixed}" || {
    echo "not ok" 1 - "mixed-case --cms-engine value was rejected"
    exit 0
}

cmp -s "${output_lower}" "${output_mixed}" || {
    echo "not ok" 1 - "mixed-case --cms-engine output diverged from lower-case"
    exit 0
}

echo "ok" 1 - "--cms-engine value matching is case insensitive"
exit 0
