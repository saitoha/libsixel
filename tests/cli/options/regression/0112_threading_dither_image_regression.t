#!/bin/sh
# Verify threading-policy dither through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_THREADING_POLICY|NULL|dither
# Registry binding: dither_threads|dither_threads_override
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
short_output="${artifact_dir}/0112-threading-dither-short-$$.six"
env_output="${artifact_dir}/0112-threading-dither-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs \
    "-=balanced:B6:D1" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "threading dither short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=dither\|stored=1\|binding=dither_threads,dither_threads_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading dither short value was not stored"
    exit 0
}
case "${short_trace}" in *LSXDTH1\|*dither_threads=1\|encode_threads=5**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading dither short did not cap the dither workers"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs \
    --env "SIXEL_THREADING_DITHER=1" \
    "-=balanced:B6" "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "threading dither environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=dither\|stored=1\|binding=dither_threads,dither_threads_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading dither environment value was not stored"
    exit 0
}
case "${env_trace}" in *LSXDTH1\|*dither_threads=1\|encode_threads=5**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading dither environment did not cap the dither workers"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs \
    --env "SIXEL_THREADING_DITHER=-1" \
    "-=balanced:B6" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "threading dither out-of-range conversion failed"
    exit 0
}
case "${range_trace}" in *LSXSUB1\|*key=dither\|stored=1**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "threading dither accepted an out-of-range environment value"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "threading dither short and environment output differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "threading dither preserves effective image output"
exit 0
