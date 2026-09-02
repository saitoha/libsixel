#!/bin/sh
# Verify builtin:bmp_info40_mode preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_BUILTIN|bmp_info40_mode
# Registry binding: builtin_bmp_info40_mode

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/bmp-info40-os2-huffman1d-2x2.bmp"
reference_image="${TOP_SRCDIR}/tests/data/inputs/formats/bmp-info40-os2-huffman1d-2x2.bmp"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0095-loader-builtin-bmp_info40_mode-short-$$.six"
env_output="${artifact_dir}/0095-loader-builtin-bmp_info40_mode-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Lbuiltin:Bos2!" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "builtin:bmp_info40_mode short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=bmp_info40_mode|stored=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "bmp_info40_mode short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE=os2" "-Lbuiltin!" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "builtin:bmp_info40_mode env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=bmp_info40_mode|stored=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "bmp_info40_mode environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "builtin:bmp_info40_mode short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "builtin:bmp_info40_mode image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "builtin:bmp_info40_mode preserves image output"
exit 0
