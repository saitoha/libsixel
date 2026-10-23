#!/bin/sh
# Verify handoff trace minimization through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIAGNOSTICS|NULL|handoff_trace
# Registry binding: handoff_trace|handoff_trace_override

# Image assertion: option-values-and-output-equivalence
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0151-handoff-short-$$.six"
env_output="${artifact_dir}/0151-handoff-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,encode_handoff \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_ENCODE_HANDOFF_TRACE_MINIMAL=0" "-xhuman:H1" \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "handoff trace short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=handoff_trace\|stored=1\|binding=handoff_trace,handoff_trace_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "handoff trace short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,encode_handoff \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_ENCODE_HANDOFF_TRACE_MINIMAL=1" -xhuman \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "handoff trace environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=handoff_trace\|stored=1\|binding=handoff_trace,handoff_trace_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "handoff trace environment value was not stored"
    exit 0
}

case "${short_trace}" in *event=callback_handoff_decide**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "handoff trace omitted its decision event"
    exit 0
}
case "${short_trace}" in *event=callback_enter**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "handoff trace retained verbose events"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "handoff trace outputs differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "handoff trace preserves image and trace output"
exit 0
