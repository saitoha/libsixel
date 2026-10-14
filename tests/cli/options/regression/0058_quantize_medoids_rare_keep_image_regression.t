#!/bin/sh
# Verify medoids:rare_keep preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS|rare_keep
# Registry binding: quantize_model_kmedoids_rare_keep|quantize_model_kmedoids_rare_keep_override
# Environment range: clamp-positive-uint

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0058-quantize-medoids-rare_keep-short-$$.six"
env_output="${artifact_dir}/0058-quantize-medoids-rare_keep-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qmedoids:R1024" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "medoids:rare_keep short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=rare_keep\|stored=1\|binding=quantize_model_kmedoids_rare_keep,quantize_model_kmedoids_rare_keep_override\|value=1024**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "rare_keep short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_RARE_KEEP=1025" -p 16 "-Qmedoids" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "medoids:rare_keep env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=rare_keep\|stored=1\|binding=quantize_model_kmedoids_rare_keep,quantize_model_kmedoids_rare_keep_override\|value=1024**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "rare_keep environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_RARE_KEEP=0" \
    -p 16 "-Qmedoids" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "medoids:rare_keep lower conversion failed"
    exit 0
}

case "${range_trace}" in *LSXSUB1\|*key=rare_keep\|stored=1\|binding=quantize_model_kmedoids_rare_keep,quantize_model_kmedoids_rare_keep_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "medoids rare_keep rejected zero"
    exit 0
}

width_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_RARE_KEEP=4294967296" \
    -p 16 "-Qmedoids" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "medoids:rare_keep width conversion failed"
    exit 0
}

case "${width_trace}" in *LSXSUB1\|*key=rare_keep\|stored=1*) false ;; *) true ;; esac || {
    echo "not ok" 1 - "medoids rare_keep accepted an over-width value"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "medoids:rare_keep short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "medoids:rare_keep preserves image output"
exit 0
