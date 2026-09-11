#!/bin/sh
# Policy: docs/loader/builtin/png.md
# Verify common loader background colorspace through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|NULL|background_colorspace
# Registry binding: background_colorspace
# Policy: docs/loader/alpha-policy.md
# Policy: docs/loader/background-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# Preserve the unmanaged fixture values; CMS defaults are tested separately.

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/images/pngsuite/background/bgan6a08.png"
# The reference composites every alpha value, including the zero-alpha first
# column, over linear #808080.
reference_image="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0084_pngsuite_background_midgray_linear_bgan6a08_msssim.ppm"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0098-loader-background-colorspace-short-$$.six"
env_output="${artifact_dir}/0098-loader-background-colorspace-env-$$.six"
control_output="${artifact_dir}/0098-loader-background-colorspace-control-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=none \
    -A composite "-Lbuiltin:Pexplicit_first:Clinear!" \
    -B#808080 "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "background_colorspace short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=background_colorspace|stored=1|binding=background_colorspace|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "background_colorspace short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=none \
    -A composite \
    --env "SIXEL_BACKGROUND_POLICY=explicit_first" \
    --env "SIXEL_LOADER_BACKGROUND_COLORSPACE=linear" "-Lbuiltin!" \
    -B#808080 "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "background_colorspace environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=background_colorspace|stored=1|binding=background_colorspace|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "background_colorspace environment value was not stored"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=none \
    -A composite \
    --env "SIXEL_BACKGROUND_POLICY=explicit_first" \
    --env "SIXEL_LOADER_BACKGROUND_COLORSPACE=gamma" "-Lbuiltin!" \
    -B#808080 "${input_image}" >"${control_output}" || {
    echo "not ok" 1 - "background_colorspace control conversion failed"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "background_colorspace short and env output differ"
    exit 0
}

cmp -s "${short_output}" "${control_output}" && {
    echo "not ok" 1 - "background_colorspace did not affect image output"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" --cms-engine=none \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "background_colorspace image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "background_colorspace preserves image output"
exit 0
