#!/bin/sh
# Verify phased:gain preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|g_diffusion_values + SIXEL_DIFFUSION_BASE_PHASED|gain
# Registry binding: phased_gain

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
short_output="${artifact_dir}/0043-diffusion-phased-gain-short-$$.six"
env_output="${artifact_dir}/0043-diffusion-phased-gain-env-$$.six"

short_trace=$(set +xv; SIXEL_GPU_POLICY=force SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --precision-policy=8bit --lookup-policy=ordered -d "phased:Gsmall-error" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "phased:gain short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=gain\|stored=1\|binding=phased_gain**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "gain short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_GPU_POLICY=force SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --precision-policy=8bit --lookup-policy=ordered --env "SIXEL_DITHER_PHASED_GAIN=small-error" -d "phased" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "phased:gain env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=gain\|stored=1\|binding=phased_gain**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "gain environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "phased:gain short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "phased:gain preserves image output"
exit 0
