#!/bin/sh
# Verify wic:ico_minsize preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_WIC|ico_minsize

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}
test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic is unavailable\\n"
    exit 0
}
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/snake-ico-multisize.ico"
reference_image="${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.ppm"
artifact_dir="${ARTIFACT_ROOT}/suboption-regression"
test -d "${artifact_dir}" || mkdir -p "${artifact_dir}"
short_output="${artifact_dir}/0096-loader-wic-ico_minsize-short.six"
env_output="${artifact_dir}/0096-loader-wic-ico_minsize-env.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Lwic:I30!" "${input_image}" -o "${short_output}" || {
    echo "not ok" 1 - "wic:ico_minsize short conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_WIC_ICO_MINSIZE=30" "-Lwic!" \
    "${input_image}" -o "${env_output}" || {
    echo "not ok" 1 - "wic:ico_minsize env conversion failed"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "wic:ico_minsize short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.96" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "wic:ico_minsize image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "wic:ico_minsize preserves image output"
exit 0
