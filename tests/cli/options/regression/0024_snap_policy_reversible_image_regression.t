#!/bin/sh
# Policy: docs/encoder/palette-construction/snap-policy/README.md
# Top-level option regression
# Verify the snap policy through short and environment paths.
# Palette contract: policy=reversible

# Image assertion: option-values-and-output-equivalence
set -eux

# OpenBSD rand() is non-deterministic by default, so pin the unrelated
# Kmeans initializer while comparing independent converter processes.
SIXEL_PALETTE_KMEANS_SEED=1
export SIXEL_PALETTE_KMEANS_SEED

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0024-snap-policy-short-$$.six"
env_output="${artifact_dir}/0024-snap-policy-env-$$.six"
none_output="${artifact_dir}/0024-snap-policy-none-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qkmeans -Woklab \
    "-_reversible" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "snap policy short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSNP1\|*policy=reversible**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "snap policy missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_SNAP_POLICY=reversible" \
    -p 16 -Qkmeans -Woklab \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "snap policy environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSNP1\|*policy=reversible**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "snap policy environment missed its consumer"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "snap policy short and environment output differ"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qkmeans -Woklab \
    --snap-policy=none "${input_image}" >"${none_output}" || {
    echo "not ok" 1 - "disabled snap policy conversion failed"
    exit 0
}
cmp -s "${short_output}" "${none_output}" && {
    echo "not ok" 1 - "reversible snap policy did not alter output"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "snap policy preserves image output"
exit 0
