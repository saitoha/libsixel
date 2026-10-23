#!/bin/sh
# Verify PSD trace-only control through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIAGNOSTICS|NULL|psd_trace
# Registry binding: psd_trace|psd_trace_override

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_layers_minimal_type_layer.psd"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0152-psd-trace-short-$$.six"
env_output="${artifact_dir}/0152-psd-trace-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,psd_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PSD_TRACE_ONLY=1" "-xhuman:D0" \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "PSD trace short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=psd_trace\|stored=1\|binding=psd_trace,psd_trace_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "PSD trace short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,psd_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PSD_TRACE_ONLY=0" -xhuman \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "PSD trace environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=psd_trace\|stored=1\|binding=psd_trace,psd_trace_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "PSD trace environment value was not stored"
    exit 0
}

test -s "${short_output}" || {
    echo "not ok" 1 - "PSD trace disable did not reach the encoder"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "PSD trace outputs differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "PSD trace control preserves decoded image output"
exit 0
