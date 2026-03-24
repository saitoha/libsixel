#!/bin/sh
# TAP test verifying SIXEL_LOADER_LIBJPEG_CMS_ENGINE overrides global setting.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-embedded-esrgb.jpg"
output_cms1="${ARTIFACT_LOCAL_DIR}/cms_engine_env_libjpeg_ref_cms1.six"
output_cms0="${ARTIFACT_LOCAL_DIR}/cms_engine_env_libjpeg_ref_cms0.six"
output_override="${ARTIFACT_LOCAL_DIR}/cms_engine_env_libjpeg_override.six"

run_img2sixel -Llibjpeg:cms=1! "${input_jpeg}" >"${output_cms1}" || {
    echo "not ok" 1 - "libjpeg cms=1 reference decode failed"
    exit 0
}

run_img2sixel -Llibjpeg:cms=0! "${input_jpeg}" >"${output_cms0}" || {
    echo "not ok" 1 - "libjpeg cms=0 reference decode failed"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.995" \
    "${output_cms1}" "${output_cms0}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 5 || {
    echo "not ok" 1 - "libjpeg cms references were not distinguishable: ${lsqa_msg-}"
    exit 0
}

run_img2sixel \
    --env "SIXEL_LOADER_CMS_ENGINE=none" \
    --env "SIXEL_LOADER_LIBJPEG_CMS_ENGINE=auto" \
    -Llibjpeg! "${input_jpeg}" >"${output_override}" || {
    echo "not ok" 1 - "SIXEL_LOADER_LIBJPEG_CMS_ENGINE override decode failed"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.999" \
    "${output_override}" "${output_cms1}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 0 || {
    echo "not ok" 1 - "SIXEL_LOADER_LIBJPEG_CMS_ENGINE did not match cms=1 reference: ${lsqa_msg-}"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.995" \
    "${output_override}" "${output_cms0}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 5 || {
    echo "not ok" 1 - "override output did not differ from cms=0 baseline: ${lsqa_msg-}"
    exit 0
}

echo "ok" 1 - "SIXEL_LOADER_LIBJPEG_CMS_ENGINE overrides global none"
exit 0
