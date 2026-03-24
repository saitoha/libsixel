#!/bin/sh
# TAP test verifying --cms-engine=lcms behaves like --cms-engine=lcms2.

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
output_lcms2="${ARTIFACT_LOCAL_DIR}/cms_engine_alias_lcms_ref_lcms2.six"
output_lcms="${ARTIFACT_LOCAL_DIR}/cms_engine_alias_lcms_alias.six"

run_img2sixel \
    --cms-engine=lcms2 \
    -Llibwebp:cms=1! "${input_webp}" >"${output_lcms2}" || {
    echo "not ok" 1 - "--cms-engine=lcms2 reference decode failed"
    exit 0
}

run_img2sixel \
    --cms-engine=lcms \
    -Llibwebp:cms=1! "${input_webp}" >"${output_lcms}" || {
    echo "not ok" 1 - "--cms-engine=lcms was rejected"
    exit 0
}

cmp -s "${output_lcms2}" "${output_lcms}" || {
    echo "not ok" 1 - "--cms-engine=lcms diverged from --cms-engine=lcms2"
    exit 0
}

echo "ok" 1 - "--cms-engine=lcms matches --cms-engine=lcms2"
exit 0
