#!/bin/sh
# Verify libwebp:orientation preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_LIBWEBP|orientation

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}
test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp is unavailable\\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_12x8.webp"
reference_image="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_12x8.webp"
artifact_dir="${ARTIFACT_ROOT}/suboption-regression"
test -d "${artifact_dir}" || mkdir -p "${artifact_dir}"
short_output="${artifact_dir}/0090-loader-libwebp-orientation-short.six"
env_output="${artifact_dir}/0090-loader-libwebp-orientation-env.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibwebp:Ooff!" "${input_image}" -o "${short_output}" || {
    echo "not ok" 1 - "libwebp:orientation short conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_LIBWEBP_ORIENTATION=off" "-Llibwebp!" \
    "${input_image}" -o "${env_output}" || {
    echo "not ok" 1 - "libwebp:orientation env conversion failed"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "libwebp:orientation short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "libwebp:orientation image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "libwebp:orientation preserves image output"
exit 0
