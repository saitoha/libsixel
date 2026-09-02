#!/bin/sh
# Verify libwebp:cms_engine preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_LIBWEBP|cms_engine

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

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.webp"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.webp"
artifact_dir="${ARTIFACT_ROOT}/suboption-regression"
test -d "${artifact_dir}" || mkdir -p "${artifact_dir}"
short_output="${artifact_dir}/0089-loader-libwebp-cms_engine-short.six"
env_output="${artifact_dir}/0089-loader-libwebp-cms_engine-env.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibwebp:Eauto!" "${input_image}" -o "${short_output}" || {
    echo "not ok" 1 - "libwebp:cms_engine short conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=auto" "-Llibwebp!" \
    "${input_image}" -o "${env_output}" || {
    echo "not ok" 1 - "libwebp:cms_engine env conversion failed"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "libwebp:cms_engine short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "libwebp:cms_engine image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "libwebp:cms_engine preserves image output"
exit 0
