#!/bin/sh
# Verify threading-policy palette through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_ENCODER_THREADING_POLICY|NULL|palette
# Registry binding: palette|palette_override

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
short_output="${artifact_dir}/0182-threading-palette-short-$$.six"
env_output="${artifact_dir}/0182-threading-palette-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs \
    "-=greedy:B6:Cframe" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "threading palette short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=palette\|stored=1\|binding=palette,palette_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading palette short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs \
    --env "SIXEL_ENCODER_THREADING_PALETTE=frame" \
    "-=greedy:B6" "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "threading palette environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=palette\|stored=1\|binding=palette,palette_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading palette environment value was not stored"
    exit 0
}

# The latest mode is reserved; it must fail instead of falling back.
status=0
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "-=greedy:B6:Clatest" \
    -o /dev/null "${input_image}" 2>/dev/null || status=$?
test "${status}" -ne 0 || {
    echo "not ok" 1 - "threading palette=latest was accepted"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "threading palette short and environment output differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "threading palette preserves effective image output"
exit 0
