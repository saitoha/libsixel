#!/bin/sh
# Verify hdr_fallback_profile through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|NULL|hdr_fallback_profile
# Registry binding: hdr_fallback_profile

# Image assertion: option-values-and-output-equivalence
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
short_output="${artifact_dir}/0112-loader-hdr-fallback-short-$$.six"
env_output="${artifact_dir}/0112-loader-hdr-fallback-env-$$.six"
control_output="${artifact_dir}/0112-loader-hdr-fallback-control-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=builtin \
    --env "SIXEL_LOADER_HDR_FALLBACK_PROFILE=invalid" \
    "-Lbuiltin:Tlinear:Fsrgb!" "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "hdr_fallback_profile short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=hdr_fallback_profile\|stored=1\|binding=hdr_fallback_profile\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "hdr_fallback_profile short value was not stored"
    exit 0
}

case "${short_trace}" in *builtin\ HDR:\ final\ controls*fallback=srgb**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "hdr_fallback_profile short value did not reach HDR"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=builtin \
    --env "SIXEL_LOADER_HDR_FALLBACK_PROFILE=srgb" \
    "-Lbuiltin:Tlinear!" "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "hdr_fallback_profile environment conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=hdr_fallback_profile\|stored=1\|binding=hdr_fallback_profile\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "hdr_fallback_profile environment value was not stored"
    exit 0
}

case "${env_trace}" in *builtin\ HDR:\ final\ controls*fallback=srgb**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "hdr_fallback_profile environment did not reach HDR"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "hdr_fallback_profile short and environment differ"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=builtin \
    --env "SIXEL_LOADER_HDR_FALLBACK_PROFILE=linear-srgb" \
    "-Lbuiltin:Tlinear!" "${input_image}" >"${control_output}" || {
    echo "not ok" 1 - "hdr_fallback_profile control conversion failed"
    exit 0
}

cmp -s "${short_output}" "${control_output}" && {
    echo "not ok" 1 - "hdr_fallback_profile did not alter HDR output"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "hdr_fallback_profile preserves HDR image output"
exit 0
