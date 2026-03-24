#!/bin/sh
# TAP test verifying per-loader CMS engine env overrides global loader default.

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
output_cms1="${ARTIFACT_LOCAL_DIR}/cms_engine_env_ref_cms1.six"
output_cms0="${ARTIFACT_LOCAL_DIR}/cms_engine_env_ref_cms0.six"
output_global_auto="${ARTIFACT_LOCAL_DIR}/cms_engine_env_global_auto.six"
output_global_none="${ARTIFACT_LOCAL_DIR}/cms_engine_env_global_none.six"
output_override="${ARTIFACT_LOCAL_DIR}/cms_engine_env_override.six"

run_img2sixel -Llibwebp:cms=1! "${input_webp}" >"${output_cms1}" || {
    echo "not ok" 1 - "libwebp cms=1 reference decode failed"
    exit 0
}

run_img2sixel -Llibwebp:cms=0! "${input_webp}" >"${output_cms0}" || {
    echo "not ok" 1 - "libwebp cms=0 reference decode failed"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.995" \
    "${output_cms1}" "${output_cms0}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 5 || {
    echo "not ok" 1 - "libwebp cms references were not distinguishable: ${lsqa_msg-}"
    exit 0
}

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=auto" \
    -Llibwebp! "${input_webp}" >"${output_global_auto}" || {
    echo "not ok" 1 - "global cms engine env (auto) failed"
    exit 0
}

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=none" \
    -Llibwebp! "${input_webp}" >"${output_global_none}" || {
    echo "not ok" 1 - "global cms engine env (none) failed"
    exit 0
}

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=none" \
    --env "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=auto" \
    -Llibwebp! "${input_webp}" >"${output_override}" || {
    echo "not ok" 1 - "per-loader cms engine override failed"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.999" \
    "${output_global_auto}" "${output_cms1}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 0 || {
    echo "not ok" 1 - "SIXEL_LOADER_CMS_ENGINE=auto did not match cms=1 reference: ${lsqa_msg-}"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.999" \
    "${output_global_none}" "${output_cms0}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 0 || {
    echo "not ok" 1 - "SIXEL_LOADER_CMS_ENGINE=none did not match cms=0 reference: ${lsqa_msg-}"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.999" \
    "${output_override}" "${output_cms1}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 0 || {
    echo "not ok" 1 - "SIXEL_LOADER_LIBWEBP_CMS_ENGINE did not override global setting: ${lsqa_msg-}"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.995" \
    "${output_override}" "${output_global_none}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 5 || {
    echo "not ok" 1 - "override output did not differ from global none baseline: ${lsqa_msg-}"
    exit 0
}

echo "ok" 1 - "per-loader cms engine env overrides global cms engine env"
exit 0
