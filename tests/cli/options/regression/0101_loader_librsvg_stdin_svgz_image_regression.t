#!/bin/sh
# Verify librsvg stdin-SVGZ policy through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_LIBRSVG|stdin_svgz
# Registry binding: librsvg_allow_stdin_svgz

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

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svgz"
reference_image="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0087_librsvg_stdin_svgz_msssim.ppm"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0101-loader-librsvg-stdin-svgz-short-$$.six"
env_output="${artifact_dir}/0101-loader-librsvg-stdin-svgz-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibrsvg:S1!" -4adaptive-grid -w16 -h16 - <"${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "stdin_svgz short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=stdin_svgz|stored=1|binding=librsvg_allow_stdin_svgz|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "stdin_svgz short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ=1" \
    "-Llibrsvg!" -4adaptive-grid -w16 -h16 - <"${input_image}" \
    2>&1 >"${env_output}") || {
    echo "not ok" 1 - "stdin_svgz environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=stdin_svgz|stored=1|binding=librsvg_allow_stdin_svgz|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "stdin_svgz environment value was not stored"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibrsvg:S0!" -4adaptive-grid -w16 -h16 - \
    <"${input_image}" >/dev/null && {
    echo "not ok" 1 - "stdin_svgz disabled control unexpectedly succeeded"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "stdin_svgz short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "stdin_svgz image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "stdin_svgz preserves image output"
exit 0
