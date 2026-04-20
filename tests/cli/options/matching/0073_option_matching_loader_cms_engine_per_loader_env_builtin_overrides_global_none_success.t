#!/bin/sh
# TAP test verifying SIXEL_LOADER_BUILTIN_CMS_ENGINE overrides global setting.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/map8_embedded_icc.png"
output_cms1="${ARTIFACT_LOCAL_DIR}/cms_engine_env_builtin_ref_cms1.six"
output_cms0="${ARTIFACT_LOCAL_DIR}/cms_engine_env_builtin_ref_cms0.six"
output_override="${ARTIFACT_LOCAL_DIR}/cms_engine_env_builtin_override.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=auto! "${input_png}" >"${output_cms1}" || {
    echo "not ok" 1 - "builtin cms=1 reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=none! "${input_png}" >"${output_cms0}" || {
    echo "not ok" 1 - "builtin cms=0 reference decode failed"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.995" \
    "${output_cms1}" "${output_cms0}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 5 || {
    echo "not ok" 1 - "builtin cms references were not distinguishable: ${lsqa_msg-}"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_CMS_ENGINE=none" \
    --env "SIXEL_LOADER_BUILTIN_CMS_ENGINE=auto" \
    -Lbuiltin! "${input_png}" >"${output_override}" || {
    echo "not ok" 1 - "SIXEL_LOADER_BUILTIN_CMS_ENGINE override decode failed"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.999" \
    "${output_override}" "${output_cms1}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 0 || {
    echo "not ok" 1 - "SIXEL_LOADER_BUILTIN_CMS_ENGINE did not match cms=1 reference: ${lsqa_msg-}"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.995" \
    "${output_override}" "${output_cms0}" 2>&1) || lsqa_status=$?
test "${lsqa_status}" -eq 5 || {
    echo "not ok" 1 - "override output did not differ from cms=0 baseline: ${lsqa_msg-}"
    exit 0
}

echo "ok" 1 - "SIXEL_LOADER_BUILTIN_CMS_ENGINE overrides global none"
exit 0
