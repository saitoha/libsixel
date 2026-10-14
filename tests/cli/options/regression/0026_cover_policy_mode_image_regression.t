#!/bin/sh
# Policy: docs/encoder/palette-construction/cover-policy/README.md
# Verify cover-policy mode preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_COVER_POLICY|NULL|cover_mode
# Registry binding: cover_policy_mode|cover_policy_mode_override
# Palette contract: mode=hard

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
short_output="${artifact_dir}/0026-quantize-heckbert-cover_mode-short-$$.six"
env_output="${artifact_dir}/0026-quantize-heckbert-cover_mode-env-$$.six"

# Keep the default 256 colors: this observes option equivalence, not the
# quality of a 16-color palette with eight slots reserved for hard anchors.
short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qheckbert "-acorners:Whard" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "cover_mode short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=cover_mode\|stored=1\|binding=cover_policy_mode,cover_policy_mode_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "cover_mode short value was not stored"
    exit 0
}

case "${short_trace}" in *LSXCOV1\|*mode=hard**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "cover_mode missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_COVER_MODE=hard" -Qheckbert -acorners \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "cover_mode env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=cover_mode\|stored=1\|binding=cover_policy_mode,cover_policy_mode_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "cover_mode environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "cover-policy mode short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "cover-policy mode preserves image output"
exit 0
