#!/bin/sh
# Verify top-level soft binning agrees across CLI and environment requests.
# Top-level option regression

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
cli_output="${artifact_dir}/0030-binning-policy-cli-$$.six"
env_output="${artifact_dir}/0030-quantize-kmeans-binning-env-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --binning-policy=soft \
    -p 16 -Qkmeans "${input_image}" >"${cli_output}" || {
    echo "not ok" 1 - "CLI soft binning conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_BINNING_POLICY=soft" -p 16 -Qkmeans \
    "${input_image}" >"${env_output}" || {
    echo "not ok" 1 - "environment soft binning conversion failed"
    exit 0
}

cmp -s "${cli_output}" "${env_output}" || {
    echo "not ok" 1 - "CLI and environment soft binning output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${cli_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "soft binning image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "top-level soft binning preserves image output"
exit 0
