#!/bin/sh
# Policy: docs/encoder/palette-construction/snap-policy/README.md
# Verify snap grid through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_SNAP_POLICY|NULL|grid
# Registry binding: snap_policy_grid|snap_policy_grid_override
# Palette contract: grid=cache

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
short_output="${artifact_dir}/0170-snap-grid-short-$$.six"
env_output="${artifact_dir}/0170-snap-grid-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 256 -Qkmeans -Woklab -Udisplay-p3 \
    "-_nearest:Gcache" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "snap grid short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=grid\|stored=1\|binding=snap_policy_grid,snap_policy_grid_override\|value=3**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "snap grid short value was not stored"
    exit 0
}
case "${short_trace}" in *LSXSNP1\|*grid=cache**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "snap grid missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_SNAP_GRID=cache" \
    -p 256 -Qkmeans -Woklab -Udisplay-p3 -_nearest \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "snap grid environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=grid\|stored=1\|binding=snap_policy_grid,snap_policy_grid_override\|value=3**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "snap grid environment value was not stored"
    exit 0
}

case "${env_trace}" in *LSXSG1\|strategy=3\|queries=[1-9]*) true ;; *) false ;; esac || {
    echo "not ok 1 - grid environment did not execute cached queries"
    exit 0
}
case "${short_trace}" in *LSXSG1\|strategy=3\|queries=[1-9]*) true ;; *) false ;; esac || {
    echo "not ok 1 - grid short form did not execute cached queries"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "snap grid short and environment output differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "snap grid preserves image output"
exit 0
