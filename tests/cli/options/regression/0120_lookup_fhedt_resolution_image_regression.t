#!/bin/sh
# Verify fhedt:resolution through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LUT_POLICY|g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT|resolution
# Registry binding: lut_policy_fhedt_resolution|lut_policy_fhedt_resolution_override
# Lookup contract: policy=fhedt|*resolution=128
# Environment range: parse-signed-choice

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0130-fhedt-resolution-short-$$.six"
env_output="${artifact_dir}/0130-fhedt-resolution-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    "-~fhedt:R128" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "fhedt:resolution short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=resolution\|stored=1\|binding=lut_policy_fhedt_resolution,lut_policy_fhedt_resolution_override\|value=128**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fhedt resolution short value was not stored"
    exit 0
}
case "${short_trace}" in *LSXLUT1\|*policy=fhedt\|*resolution=128**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fhedt resolution did not reach the policy"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env "SIXEL_LOOKUP_FHEDT_RESOLUTION=+128" "-~fhedt" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "fhedt:resolution environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=resolution\|stored=1\|binding=lut_policy_fhedt_resolution,lut_policy_fhedt_resolution_override\|value=128**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fhedt resolution environment value was not stored"
    exit 0
}
case "${env_trace}" in *LSXLUT1\|*policy=fhedt\|*resolution=128**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "environment resolution missed the policy"
    exit 0
}
float_trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --precision-policy=float32 "-~fhedt:R128" "${input_image}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "fhedt resolution float32 conversion failed"
    exit 0
}
case "${float_trace}" in *LSXLUT1\|*policy=fhedt\|precision=float32\|*resolution=128**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fhedt resolution missed the float32 backend"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "fhedt resolution outputs differ"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env "SIXEL_LOOKUP_FHEDT_RESOLUTION=65" "-~fhedt" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "fhedt resolution range conversion failed"
    exit 0
}
case "${range_trace}" in *LSXSUB1\|*key=resolution\|stored=1**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "fhedt resolution accepted a non-choice value"
    exit 0
}
case "${range_trace}" in *LSXLUT1\|*policy=fhedt\|*resolution=64**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fhedt resolution did not retain its default"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "fhedt:resolution preserves effective image output"
exit 0
