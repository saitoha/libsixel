#!/bin/sh
# Verify threading-policy pin through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_THREADING_POLICY|NULL|pin
# Registry binding: pin|pin_override

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
short_output="${artifact_dir}/0113-threading-pin-short-$$.six"
env_output="${artifact_dir}/0113-threading-pin-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs \
    "-=6:I0" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "threading pin short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=pin\|stored=1\|binding=pin,pin_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading pin short value was not stored"
    exit 0
}
case "${short_trace}" in *LSXDTH1\|*pin_threads=0\|pin_threads_override=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading pin short did not disable worker affinity"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs \
    --env "SIXEL_THREADING_PIN=0" \
    "-=6" "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "threading pin environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=pin\|stored=1\|binding=pin,pin_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading pin environment value was not stored"
    exit 0
}
case "${env_trace}" in *LSXDTH1\|*pin_threads=0\|pin_threads_override=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading pin environment did not disable worker affinity"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "threading pin short and environment output differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "threading pin preserves effective image output"
exit 0
