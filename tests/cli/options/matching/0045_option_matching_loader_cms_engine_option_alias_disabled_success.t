#!/bin/sh
# TAP test verifying --cms-engine=disabled behaves like --cms-engine=none.

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
output_none="${ARTIFACT_LOCAL_DIR}/cms_engine_alias_disabled_ref_none.six"
output_disabled="${ARTIFACT_LOCAL_DIR}/cms_engine_alias_disabled_alias.six"

run_img2sixel \
    --cms-engine=none \
    -Llibwebp:cms=1! "${input_webp}" >"${output_none}" || {
    echo "not ok" 1 - "--cms-engine=none reference decode failed"
    exit 0
}

run_img2sixel \
    --cms-engine=disabled \
    -Llibwebp:cms=1! "${input_webp}" >"${output_disabled}" || {
    echo "not ok" 1 - "--cms-engine=disabled was rejected"
    exit 0
}

cmp -s "${output_none}" "${output_disabled}" || {
    echo "not ok" 1 - "--cms-engine=disabled diverged from --cms-engine=none"
    exit 0
}

echo "ok" 1 - "--cms-engine=disabled matches --cms-engine=none"
exit 0
