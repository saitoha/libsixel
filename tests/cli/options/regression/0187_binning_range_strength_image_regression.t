#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify auto:strength preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_BINNING_POLICY|g_palette_binning_values|strength
# Registry binding: palette_range_strength|palette_range_strength_override

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
short_output="${artifact_dir}/0187-binning-range-strength-short-$$.six"
env_output="${artifact_dir}/0187-binning-range-strength-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -5 "auto:S25" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "auto:strength short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=strength\|stored=1\|binding=palette_range_strength,palette_range_strength_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "strength short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_BINNING_RANGE_STRENGTH=25" -5 "auto" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "auto:strength env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=strength\|stored=1\|binding=palette_range_strength,palette_range_strength_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "strength environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "auto:strength short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "auto:strength preserves image output"
exit 0
