#!/bin/sh
# Verify libjpeg:orientation preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_LIBJPEG|orientation
# Registry binding: libjpeg_enable_orientation

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}
test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg is unavailable\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_12x8.jpg"
reference_image="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_12x8.jpg"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0088-loader-libjpeg-orientation-short-$$.six"
env_output="${artifact_dir}/0088-loader-libjpeg-orientation-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibjpeg:O0!" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "libjpeg:orientation short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=orientation|stored=1|binding=libjpeg_enable_orientation*}" != "${short_trace}" || {
    echo "not ok" 1 - "orientation short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_LIBJPEG_ORIENTATION=0" "-Llibjpeg!" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "libjpeg:orientation env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=orientation|stored=1|binding=libjpeg_enable_orientation*}" != "${env_trace}" || {
    echo "not ok" 1 - "orientation environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "libjpeg:orientation short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "libjpeg:orientation image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "libjpeg:orientation preserves image output"
exit 0
