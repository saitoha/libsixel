#!/bin/sh
# Verify librsvg relative-resource policy through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_LIBRSVG|relative_resources
# Registry binding: librsvg_allow_relative_resources

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-relative-image.svg"
reference_image="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0086_librsvg_relative_resources_msssim.ppm"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0100-loader-librsvg-relative-short-$$.six"
env_output="${artifact_dir}/0100-loader-librsvg-relative-env-$$.six"
control_output="${artifact_dir}/0100-loader-librsvg-relative-control-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibrsvg:A1!" -w16 -h16 "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "relative_resources short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=relative_resources|stored=1|binding=librsvg_allow_relative_resources|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "relative_resources short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_LIBRSVG_ALLOW_RELATIVE_RESOURCES=1" \
    "-Llibrsvg!" -w16 -h16 "${input_image}" \
    2>&1 >"${env_output}") || {
    echo "not ok" 1 - "relative_resources environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=relative_resources|stored=1|binding=librsvg_allow_relative_resources|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "relative_resources environment value was not stored"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibrsvg:A0!" -w16 -h16 "${input_image}" \
    >"${control_output}" || {
    echo "not ok" 1 - "relative_resources control conversion failed"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "relative_resources short and env output differ"
    exit 0
}

cmp -s "${short_output}" "${control_output}" && {
    echo "not ok" 1 - "relative_resources did not affect image output"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "relative_resources image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "relative_resources preserves image output"
exit 0
