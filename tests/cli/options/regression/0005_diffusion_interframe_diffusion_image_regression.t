#!/bin/sh
# Verify interframe:diffusion preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|g_diffusion_values + SIXEL_DIFFUSION_BASE_INTERFRAME|diffusion
# Registry binding: interframe_spatial_diffuse|interframe_spatial_diffuse_override
# Dither contract: spatial=atkinson|spatial_override=1

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
animated_image="${TOP_SRCDIR}/tests/data/inputs/snake_motion_64_2frame.gif"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0005-diffusion-interframe-diffusion-short-$$.six"
env_output="${artifact_dir}/0005-diffusion-interframe-diffusion-env-$$.six"

effect_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 -L builtin -ldisable -p 64 \
    -d "interframe:Datkinson" "${animated_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "diffusion interframe diffusion animated conversion failed"
    exit 0
}

case "${effect_trace}" in *LSXDTH1\|*consume=[1-9]**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "diffusion interframe diffusion did not consume a second frame"
    exit 0
}

case "${effect_trace}" in *LSXDTH1\|*spatial=atkinson\|spatial_override=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "interframe diffusion did not reach the dither"
    exit 0
}

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 -L builtin -ldisable -p 64 \
    -d "interframe:Datkinson" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "interframe:diffusion short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=diffusion\|stored=1\|binding=interframe_spatial_diffuse,interframe_spatial_diffuse_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "diffusion short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_INTERFRAME_DIFFUSION=atkinson" \
    --threads=1 -L builtin -ldisable -p 64 \
    -d "interframe" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "interframe:diffusion env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=diffusion\|stored=1\|binding=interframe_spatial_diffuse,interframe_spatial_diffuse_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "diffusion environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "interframe:diffusion short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "interframe:diffusion preserves image output"
exit 0
