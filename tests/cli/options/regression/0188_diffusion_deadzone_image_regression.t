#!/bin/sh
# Verify fs:deadzone through short and environment paths.
# Policy: docs/encoder/palette-application/dithering/README.md
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|SIXEL_DIFFUSION_BASE_SET_CARRY|deadzone
# Registry binding: diffusion_deadzone|diffusion_deadzone_override
# Dither contract: diffuse=fs|*deadzone=0|deadzone_override=1

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v


# Exact stream comparisons require the harness-assigned output directory.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0188-deadzone-short-$$.six"
env_output="${artifact_dir}/0188-deadzone-env-$$.six"
default_output="${artifact_dir}/0188-deadzone-default-$$.six"
short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "fs:D0" \
    --precision=8bit "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "fs:deadzone short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=deadzone\|stored=1\|binding=diffusion_deadzone,diffusion_deadzone_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fs deadzone short value was not stored"
    exit 0
}

case "${short_trace}" in *LSXDTH1\|*diffuse=fs\|*deadzone=0\|deadzone_override=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fs deadzone did not reach the dither"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_DEADZONE=0" -d "fs" \
    --precision=8bit "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "fs:deadzone environment conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=deadzone\|stored=1\|binding=diffusion_deadzone,diffusion_deadzone_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fs deadzone environment value was not stored"
    exit 0
}

case "${env_trace}" in *LSXDTH1\|*diffuse=fs\|*deadzone=0\|deadzone_override=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fs environment deadzone did not reach the dither"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "fs deadzone short and environment output differ"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "fs" \
    --precision=8bit "${input_image}" >"${default_output}" || {
    echo "not ok" 1 - "fs default 8-bit conversion failed"
    exit 0
}

cmp -s "${short_output}" "${default_output}" && {
    echo "not ok" 1 - "fs deadzone did not affect 8-bit output"
    exit 0
}

# The observation is option routing and exact output equivalence.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision=8bit \
    --env "SIXEL_DITHER_DEADZONE=100" -d "fs:deadzone=0" \
    "${input_image}" >"${env_output}" || {
    echo "not ok 1 - CLI override conversion failed"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - explicit CLI deadzone must win"
    exit 0
}
echo "ok 1 - deadzone image routing and precedence"
exit 0
