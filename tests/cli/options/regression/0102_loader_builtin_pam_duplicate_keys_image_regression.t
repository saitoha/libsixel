#!/bin/sh
# Verify PAM duplicate-key policy through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_BUILTIN|pam_duplicate_keys
# Registry binding: builtin_pam_duplicate_keys

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/pam-duplicate-width-1x1.pam"
reference_image="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0088_pnm_rgb_abc_msssim.ppm"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0102-pam_duplicate_keys-short-$$.six"
env_output="${artifact_dir}/0102-pam_duplicate_keys-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Lbuiltin:D1:N1!" -w16 -h16 "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "pam_duplicate_keys short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=pam_duplicate_keys|stored=1|binding=builtin_pam_duplicate_keys|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "pam_duplicate_keys short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_PNM_ALLOW_TRAILING_DATA=1" \
    --env "SIXEL_LOADER_PAM_ALLOW_DUPLICATE_REQUIRED_KEYS=1" \
    "-Lbuiltin!" -w16 -h16 "${input_image}" \
    2>&1 >"${env_output}") || {
    echo "not ok" 1 - "pam_duplicate_keys environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=pam_duplicate_keys|stored=1|binding=builtin_pam_duplicate_keys|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "pam_duplicate_keys environment value was not stored"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Lbuiltin:D0:N1!" -w16 -h16 "${input_image}" \
    >/dev/null && {
    echo "not ok" 1 - "pam_duplicate_keys disabled control unexpectedly succeeded"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "pam_duplicate_keys short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "pam_duplicate_keys image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "pam_duplicate_keys preserves image output"
exit 0
