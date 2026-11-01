#!/bin/sh
# Verify threading-policy decode through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_ENCODER_THREADING_POLICY|NULL|decode
# Registry binding: decode_threads|decode_threads_override
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
input_image="${TOP_SRCDIR}/images/map8.six"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0184-threading-decode-short-$$.six"
env_output="${artifact_dir}/0184-threading-decode-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --loaders=builtin! -d none \
    "-=balanced:S2" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "threading decode short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=decode\|stored=1\|binding=decode_threads,decode_threads_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading decode short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --loaders=builtin! -d none \
    --env "SIXEL_ENCODER_THREADING_DECODE=2" \
    "-=balanced" "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "threading decode environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=decode\|stored=1\|binding=decode_threads,decode_threads_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading decode environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --loaders=builtin! -d none \
    --env "SIXEL_ENCODER_THREADING_DECODE=-1" \
    "-=balanced" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "threading decode out-of-range conversion failed"
    exit 0
}
case "${range_trace}" in *LSXSUB1\|*key=decode\|stored=1**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "threading decode accepted an out-of-range environment value"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "threading decode short and environment output differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "threading decode preserves effective image output"
exit 0
