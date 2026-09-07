#!/bin/sh
# Verify common loader background policy through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|NULL|background_policy
# Registry binding: background_policy
# Policy: docs/loader/background-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/images/pngsuite/background/bgbn4a08.png"
reference_image="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0072_pngsuite_background_white_bgbn4a08_msssim.ppm"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0097-loader-background-policy-short-$$.six"
env_output="${artifact_dir}/0097-loader-background-policy-env-$$.six"
control_output="${artifact_dir}/0097-loader-background-policy-control-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_BACKGROUND_POLICY=file_first" \
    "-Lbuiltin:Pexplicit_first!" -B#fff "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "background_policy short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=background_policy|stored=1|binding=background_policy|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "background_policy short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_BACKGROUND_POLICY=explicit_first" "-Lbuiltin!" \
    -B#fff "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "background_policy environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=background_policy|stored=1|binding=background_policy|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "background_policy environment value was not stored"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_BACKGROUND_POLICY=file_first" "-Lbuiltin!" \
    -B#fff "${input_image}" >"${control_output}" || {
    echo "not ok" 1 - "background_policy control conversion failed"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "background_policy short and env output differ"
    exit 0
}

cmp -s "${short_output}" "${control_output}" && {
    echo "not ok" 1 - "background_policy did not affect image output"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "background_policy image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "background_policy preserves image output"
exit 0
