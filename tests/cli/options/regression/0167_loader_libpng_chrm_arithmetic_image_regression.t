#!/bin/sh
# Verify libpng:chrm_arithmetic preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_LIBPNG|chrm_arithmetic
# Registry binding: libpng_chrm_arithmetic
# Policy: docs/loader/libpng.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}
test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is unavailable\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0167-loader-libpng-chrm_arithmetic-short-$$.six"
env_output="${artifact_dir}/0167-loader-libpng-chrm_arithmetic-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibpng:A512bit!" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "libpng:chrm_arithmetic short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=chrm_arithmetic\|stored=1\|binding=libpng_chrm_arithmetic**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "chrm_arithmetic short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_LIBPNG_CHRM_ARITHMETIC=512bit" "-Llibpng!" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "libpng:chrm_arithmetic env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=chrm_arithmetic\|stored=1\|binding=libpng_chrm_arithmetic**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "chrm_arithmetic environment value was not stored"
    exit 0
}

case "${short_trace}" in *PNGCHRM1\|*requested=512bit\|selected=512bit\|actual=identity**) true ;; *) false ;; esac || {
    echo "not ok 1 - forced arithmetic did not reach the cHRM consumer"
    exit 0
}

case "${env_trace}" in *PNGCHRM1\|*requested=512bit\|selected=512bit\|actual=identity**) true ;; *) false ;; esac || {
    echo "not ok 1 - environment arithmetic did not reach the cHRM consumer"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "libpng:chrm_arithmetic short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "libpng:chrm_arithmetic image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "libpng:chrm_arithmetic preserves image output"
exit 0
