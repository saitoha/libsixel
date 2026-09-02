#!/bin/sh
# Verify libpng:orientation preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_LIBPNG|orientation
# Registry binding: libpng_enable_orientation

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

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_12x8.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_12x8.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0086-loader-libpng-orientation-short-$$.six"
env_output="${artifact_dir}/0086-loader-libpng-orientation-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibpng:O0!" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "libpng:orientation short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=orientation|stored=1|binding=libpng_enable_orientation*}" != "${short_trace}" || {
    echo "not ok" 1 - "orientation short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_LIBPNG_ORIENTATION=0" "-Llibpng!" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "libpng:orientation env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=orientation|stored=1|binding=libpng_enable_orientation*}" != "${env_trace}" || {
    echo "not ok" 1 - "orientation environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "libpng:orientation short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "libpng:orientation image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "libpng:orientation preserves image output"
exit 0
