#!/bin/sh
# TAP test verifying global env alias disabled maps to none.

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
output_ref_cms1="${ARTIFACT_LOCAL_DIR}/cms_engine_global_disabled_ref_cms1.six"
output_ref_none="${ARTIFACT_LOCAL_DIR}/cms_engine_global_disabled_ref_none.six"
output_env_disabled="${ARTIFACT_LOCAL_DIR}/cms_engine_global_disabled_alias.six"

run_img2sixel -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_ref_cms1}" || {
    echo "not ok" 1 - "cms=1 reference decode failed"
    exit 0
}

run_img2sixel -Llibwebp:cms_engine=none! "${input_webp}" >"${output_ref_none}" || {
    echo "not ok" 1 - "cms=0 reference decode failed"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.995" \
    "${output_ref_cms1}" "${output_ref_none}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 5 || {
    echo "not ok" 1 - "webp cms references were not distinguishable: ${lsqa_msg-}"
    exit 0
}

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=disabled" \
    -Llibwebp! "${input_webp}" >"${output_env_disabled}" || {
    echo "not ok" 1 - "SIXEL_LOADER_CMS_ENGINE=disabled decode failed"
    exit 0
}

cmp -s "${output_ref_none}" "${output_env_disabled}" || {
    echo "not ok" 1 - "SIXEL_LOADER_CMS_ENGINE=disabled did not map to none behavior"
    exit 0
}

echo "ok" 1 - "SIXEL_LOADER_CMS_ENGINE=disabled maps to none behavior"
exit 0
