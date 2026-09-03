#!/bin/sh
# Verify hdr_exposure through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|NULL|hdr_exposure
# Registry binding: hdr_exposure_ev

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_gradient16x16.hdr"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0113-loader-hdr-exposure-short-$$.six"
env_output="${artifact_dir}/0113-loader-hdr-exposure-env-$$.six"
control_output="${artifact_dir}/0113-loader-hdr-exposure-control-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=builtin \
    --env "SIXEL_LOADER_HDR_EXPOSURE_EV=invalid" \
    "-Lbuiltin:Tlinear:X1!" "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "hdr_exposure short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=hdr_exposure|stored=1|binding=hdr_exposure_ev|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "hdr_exposure short value was not stored"
    exit 0
}

test "${short_trace#*builtin HDR: final controls*env_ev=1.000000*}" != "${short_trace}" || {
    echo "not ok" 1 - "hdr_exposure short value did not reach HDR"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=builtin \
    --env "SIXEL_LOADER_HDR_EXPOSURE_EV=1" \
    "-Lbuiltin:Tlinear!" "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "hdr_exposure environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=hdr_exposure|stored=1|binding=hdr_exposure_ev|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "hdr_exposure environment value was not stored"
    exit 0
}

test "${env_trace#*builtin HDR: final controls*env_ev=1.000000*}" != "${env_trace}" || {
    echo "not ok" 1 - "hdr_exposure environment value did not reach HDR"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "hdr_exposure short and environment differ"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=builtin \
    --env "SIXEL_LOADER_HDR_EXPOSURE_EV=0" \
    "-Lbuiltin:Tlinear!" "${input_image}" >"${control_output}" || {
    echo "not ok" 1 - "hdr_exposure control conversion failed"
    exit 0
}

cmp -s "${short_output}" "${control_output}" && {
    echo "not ok" 1 - "hdr_exposure did not alter HDR output"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${env_output}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "hdr_exposure image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "hdr_exposure preserves HDR image output"
exit 0
