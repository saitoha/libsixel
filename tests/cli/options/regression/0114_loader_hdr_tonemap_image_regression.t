#!/bin/sh
# Verify hdr_tonemap through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|NULL|hdr_tonemap
# Registry binding: hdr_tonemap_mode

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
short_output="${artifact_dir}/0114-loader-hdr-tonemap-short-$$.six"
env_output="${artifact_dir}/0114-loader-hdr-tonemap-env-$$.six"
control_output="${artifact_dir}/0114-loader-hdr-tonemap-control-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=builtin \
    --env "SIXEL_LOADER_HDR_TONEMAP=invalid" \
    "-Lbuiltin:Tlinear:X2:Hreinhard!" "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "hdr_tonemap short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=hdr_tonemap|stored=1|binding=hdr_tonemap_mode|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "hdr_tonemap short value was not stored"
    exit 0
}

test "${short_trace#*builtin HDR: final controls*tonemap=reinhard*}" != "${short_trace}" || {
    echo "not ok" 1 - "hdr_tonemap short value did not reach HDR"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=builtin \
    --env "SIXEL_LOADER_HDR_TONEMAP=reinhard" \
    "-Lbuiltin:Tlinear:X2!" "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "hdr_tonemap environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=hdr_tonemap|stored=1|binding=hdr_tonemap_mode|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "hdr_tonemap environment value was not stored"
    exit 0
}

test "${env_trace#*builtin HDR: final controls*tonemap=reinhard*}" != "${env_trace}" || {
    echo "not ok" 1 - "hdr_tonemap environment value did not reach HDR"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "hdr_tonemap short and environment differ"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=builtin \
    --env "SIXEL_LOADER_HDR_TONEMAP=none" \
    "-Lbuiltin:Tlinear:X2!" "${input_image}" >"${control_output}" || {
    echo "not ok" 1 - "hdr_tonemap control conversion failed"
    exit 0
}

cmp -s "${short_output}" "${control_output}" && {
    echo "not ok" 1 - "hdr_tonemap did not alter HDR output"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${env_output}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "hdr_tonemap image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "hdr_tonemap preserves HDR image output"
exit 0
