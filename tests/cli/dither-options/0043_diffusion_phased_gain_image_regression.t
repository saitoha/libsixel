#!/bin/sh
# Verify lso2/phased gain preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|SIXEL_DIFFUSION_BASE_SET_GPU_LSO2|gain
# Registry binding: phased_gain|phased_gain_override
# Dither contract: diffuse=phased

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette phased/gpu_off_rejected || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP Metal device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - GPU capability probe failed"
    exit 0
}

# These output files are compared; the harness supplies only a path.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0043-diffusion-lso2-gain-short-$$.six"
env_output="${artifact_dir}/0043-diffusion-lso2-gain-env-$$.six"

short_trace=$(set +xv; SIXEL_GPU_POLICY=force \
    SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --precision-policy=8bit --lookup-policy=ordered -d "lso2:Gsmall-error" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "lso2:gain short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=gain\|stored=1\|binding=phased_gain,phased_gain_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "lso2 gain short value was not stored"
    exit 0
}

case "${short_trace}" in *LSXDTH1\|*diffuse=phased**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "lso2 short path did not select the GPU solver"
    exit 0
}

env_trace=$(set +xv; SIXEL_GPU_POLICY=force \
    SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --precision-policy=8bit --lookup-policy=ordered --env "SIXEL_DITHER_PHASED_GAIN=small-error" -d "lso2" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "lso2:gain env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=gain\|stored=1\|binding=phased_gain,phased_gain_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "lso2 gain environment value was not stored"
    exit 0
}

case "${env_trace}" in *LSXDTH1\|*diffuse=phased**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "lso2 environment path did not select the GPU solver"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "lso2:gain short and env output differ"
    exit 0
}

test -s "${short_output}" || {
    echo "not ok 1 - lso2 option output is empty"
    exit 0
}

alias_output="${artifact_dir}/0043-diffusion-phased-gain-alias-$$.six"
alias_trace=$(set +xv; SIXEL_GPU_POLICY=force SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --precision-policy=8bit --lookup-policy=ordered -d "phased:Gsmall-error" "${input_image}" 2>&1 >"${alias_output}") || {
    echo "not ok" 1 - "phased alias conversion failed"
    exit 0
}

case "${alias_trace}" in *LSXDTH1\|*diffuse=phased**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "phased alias did not select the GPU solver"
    exit 0
}

# phased is a deprecated spelling of lso2 and must select the same output.
cmp -s "${short_output}" "${alias_output}" || {
    echo "not ok" 1 - "lso2 and phased gain output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.

echo "ok" 1 - "lso2/phased gain preserves image output"
exit 0
