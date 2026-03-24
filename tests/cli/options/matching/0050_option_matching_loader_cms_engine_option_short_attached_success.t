#!/bin/sh
# TAP test verifying short attached -#VALUE form is accepted.

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
output_equals="${ARTIFACT_LOCAL_DIR}/cms_engine_short_attached_ref_equals.six"
output_attached="${ARTIFACT_LOCAL_DIR}/cms_engine_short_attached_actual.six"

run_img2sixel \
    --cms-engine=auto \
    -Llibwebp:cms=1! "${input_webp}" >"${output_equals}" || {
    echo "not ok" 1 - "--cms-engine=auto reference decode failed"
    exit 0
}

run_img2sixel \
    -#auto \
    -Llibwebp:cms=1! "${input_webp}" >"${output_attached}" || {
    echo "not ok" 1 - "short attached -#auto form was rejected"
    exit 0
}

cmp -s "${output_equals}" "${output_attached}" || {
    echo "not ok" 1 - "short attached -#auto output diverged from reference"
    exit 0
}

echo "ok" 1 - "short attached -#VALUE form works"
exit 0
