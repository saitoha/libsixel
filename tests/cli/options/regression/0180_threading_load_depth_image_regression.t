#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Verify threading-policy load_depth through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_ENCODER_THREADING_POLICY|NULL|load_depth
# Registry binding: load_depth|load_depth_override
# Environment range: parse-signed-long

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0180-threading-load-depth-short-$$.six"
env_output="${artifact_dir}/0180-threading-load-depth-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -d fs \
    "-=greedy:B6:Q2" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "threading load_depth short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=load_depth\|stored=1\|binding=load_depth,load_depth_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading load_depth short value was not stored"
    exit 0
}
case "${short_trace}" in *threading:\ *load=ahead\ load_depth=2\ **) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading load_depth short did not set the loader queue depth"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -d fs \
    --env "SIXEL_ENCODER_THREADING_LOAD_DEPTH=2" \
    "-=greedy:B6" "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "threading load_depth environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=load_depth\|stored=1\|binding=load_depth,load_depth_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading load_depth environment value was not stored"
    exit 0
}
case "${env_trace}" in *threading:\ *load=ahead\ load_depth=2\ **) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading load_depth environment did not set the loader queue depth"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -d fs \
    --env "SIXEL_ENCODER_THREADING_LOAD_DEPTH=17" \
    "-=greedy:B6" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "threading load_depth out-of-range conversion failed"
    exit 0
}
case "${range_trace}" in *LSXSUB1\|*key=load_depth\|stored=1**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "threading load_depth accepted an out-of-range environment value"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "threading load_depth short and environment output differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "threading load_depth preserves effective image output"
exit 0
