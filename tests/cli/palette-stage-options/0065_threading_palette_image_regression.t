#!/bin/sh
# Verify accepted palette routes and effective frame output.
# Policy: docs/cli/threading-policy.md
# Registry row: SIXEL_OPTION_SCHEMA_ENCODER_THREADING_POLICY|NULL|palette
# Registry binding: palette|palette_override

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
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
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -d fs --lookup-policy=none \
    "-=greedy:B6:Clatest" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "threading palette short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=palette\|stored=1\|binding=palette,palette_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading palette short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -d fs --lookup-policy=none \
    --env "SIXEL_ENCODER_THREADING_PALETTE=latest" \
    "-=greedy:B6" "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "threading palette environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=palette\|stored=1\|binding=palette,palette_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading palette environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "threading palette short and environment output differ"
    exit 0
}
case "${env_trace}" in *"palette=latest palette_effective=frame"*) true ;; *) false ;; esac || {
    echo "not ok 1 - latest did not reach the effective frame path"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs --lookup-policy=none \
    --threading-policy=greedy:B6:palette=latest "${input_image}" \
    >"${env_output}" || {
    echo "not ok 1 - long latest conversion failed"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - compact and long latest outputs differ"
    exit 0
}
env_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -v -d fs --lookup-policy=none \
    --env "SIXEL_ENCODER_THREADING_PALETTE=latest" \
    "-=greedy:B6:palette=frame" "${input_image}" \
    2>&1 >"${env_output}") || {
    echo "not ok 1 - explicit frame override conversion failed"
    exit 0
}
case "${env_trace}" in *"palette=frame palette_effective=frame"*) true ;; *) false ;; esac || {
    echo "not ok 1 - explicit frame did not override latest environment"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - latest fallback and explicit frame outputs differ"
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
