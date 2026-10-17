#!/bin/sh
# Verify heckbert:order preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_HECKBERT|order
# Registry binding: heckbert_order|heckbert_order_override

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

# Exact output comparison needs the runner-provided isolated artifact path.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"



input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0186-quantize-heckbert-order-short-$$.six"
env_output="${artifact_dir}/0186-quantize-heckbert-order-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qheckbert:Osse" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "heckbert:order short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=order\|stored=1\|binding=heckbert_order,heckbert_order_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "order short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_HECKBERT_ORDER=sse" -p 16 "-Qheckbert" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "heckbert:order env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=order\|stored=1\|binding=heckbert_order,heckbert_order_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "order environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "heckbert:order short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "heckbert:order preserves image output"
exit 0
