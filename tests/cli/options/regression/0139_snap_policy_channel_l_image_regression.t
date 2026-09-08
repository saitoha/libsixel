#!/bin/sh
# Policy: docs/functionality/snap-policy.md
# Verify snap channel weight through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_SNAP_POLICY|NULL|channel_l
# Registry binding: snap_policy_channel_factor_l|snap_policy_channel_factor_l_override
# Environment range: clamp-both
# Palette contract: channel_l=0.75

set -eux

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
test "${short_trace#*LSXSUB1|*key=channel_l|stored=1|binding=snap_policy_channel_factor_l,snap_policy_channel_factor_l_override|value=0.75*}" != "${short_trace}" || {
    echo "not ok" 1 - "snap channel short value was not stored"
    exit 0
}
test "${short_trace#*LSXSNP1|*channel_l=0.75*}" != "${short_trace}" || {
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
test "${env_trace#*LSXSUB1|*key=channel_l|stored=1|binding=snap_policy_channel_factor_l,snap_policy_channel_factor_l_override|value=0.75*}" != "${env_trace}" || {
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
test "${range_trace#*LSXSUB1|*key=channel_l|stored=1|binding=snap_policy_channel_factor_l,snap_policy_channel_factor_l_override|value=0*}" != "${range_trace}" || {
    echo "not ok" 1 - "snap channel lower endpoint was not clamped"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "snap channel short and environment output differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "snap channel image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "snap channel weight preserves image output"
exit 0
