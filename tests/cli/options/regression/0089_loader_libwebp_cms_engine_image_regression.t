#!/bin/sh
# Verify libwebp:cms_engine preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_LIBWEBP|cms_engine
# Registry binding: libwebp_cms_engine

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

input_image="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_mab_valid.webp"
reference_image="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgb_mab_valid_webp_builtin.six"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0089-loader-libwebp-cms_engine-short-$$.six"
env_output="${artifact_dir}/0089-loader-libwebp-cms_engine-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibwebp:Ebuiltin!" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "libwebp:cms_engine short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=cms_engine|stored=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "cms_engine short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_LIBWEBP_CMS_ENGINE=builtin" "-Llibwebp!" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "libwebp:cms_engine env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=cms_engine|stored=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "cms_engine environment value was not stored"
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
