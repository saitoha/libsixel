#!/bin/sh
# Verify libjpeg:orientation preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_LIBJPEG|orientation

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

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_12x8.jpg"
reference_image="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_12x8.jpg"
artifact_dir="${ARTIFACT_ROOT}/suboption-regression"
test -d "${artifact_dir}" || mkdir -p "${artifact_dir}"
short_output="${artifact_dir}/0088-loader-libjpeg-orientation-short.six"
env_output="${artifact_dir}/0088-loader-libjpeg-orientation-env.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibjpeg:Ooff!" "${input_image}" -o "${short_output}" || {
    echo "not ok" 1 - "libjpeg:orientation short conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_LIBJPEG_ORIENTATION=off" "-Llibjpeg!" \
    "${input_image}" -o "${env_output}" || {
    echo "not ok" 1 - "libjpeg:orientation env conversion failed"
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
