#!/bin/sh
# Policy: docs/encoder/palette-construction/snap-policy/README.md
# Verify snap channel weight through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_SNAP_POLICY|NULL|channel_l
# Registry binding: snap_policy_channel_factor_l|snap_policy_channel_factor_l_override
# Environment range: clamp-both
# Palette contract: channel_l=0.75

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
short_output="${artifact_dir}/0139-snap-channel-short-$$.six"
env_output="${artifact_dir}/0139-snap-channel-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qkmeans -Woklab \
    "-_nearest:L0.75" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "snap channel short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=channel_l\|stored=1\|binding=snap_policy_channel_factor_l,snap_policy_channel_factor_l_override\|value=0.75**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "snap channel short value was not stored"
    exit 0
}
case "${short_trace}" in *LSXSNP1\|*channel_l=0.75**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "snap channel weight missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L=0.75" \
    -p 16 -Qkmeans -Woklab -_nearest \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "snap channel environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=channel_l\|stored=1\|binding=snap_policy_channel_factor_l,snap_policy_channel_factor_l_override\|value=0.75**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "snap channel environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L=-1" \
    -p 16 -Qkmeans -_nearest "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "snap channel lower conversion failed"
    exit 0
}
case "${range_trace}" in *LSXSUB1\|*key=channel_l\|stored=1\|binding=snap_policy_channel_factor_l,snap_policy_channel_factor_l_override\|value=0**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "snap channel lower endpoint was not clamped"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "snap channel short and environment output differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "snap channel weight preserves image output"
exit 0
