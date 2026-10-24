#!/bin/sh
# Policy: docs/threading/policy-modes.md
# Verify threading-policy load through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_THREADING_POLICY|NULL|load
# Registry binding: load|load_override

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
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_motion_64_2frame.gif"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0179-threading-load-short-$$.six"
env_output="${artifact_dir}/0179-threading-load-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -g -l disable -d fs \
    "-=balanced:B6:Lserial" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "threading load short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=load\|stored=1\|binding=load,load_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading load short value was not stored"
    exit 0
}
case "${short_trace}" in *threading:\ *load=serial\ **) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading load short did not keep frame loading serial"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -g -l disable -d fs \
    --env "SIXEL_THREADING_LOAD=serial" \
    "-=balanced:B6" "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "threading load environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=load\|stored=1\|binding=load,load_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading load environment value was not stored"
    exit 0
}
case "${env_trace}" in *threading:\ *load=serial\ **) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading load environment did not keep frame loading serial"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "threading load short and environment output differ"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -g -l disable -d fs \
    "-=balanced:B6:Lahead" "${input_image}" -o "${env_output}" || {
    echo "not ok 1 - read-ahead playback failed"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - load concurrency changed animation output"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "threading load preserves effective image output"
exit 0
