#!/bin/sh
# Policy: docs/functionality/snap-policy.md
# Verify snap rate through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_SNAP_POLICY|NULL|rate
# Registry binding: snap_policy_approach_rate|snap_policy_approach_rate_override
# Environment range: clamp-both
# Palette contract: approach=0.5

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
short_output="${artifact_dir}/0138-snap-rate-short-$$.six"
env_output="${artifact_dir}/0138-snap-rate-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qkmeans -Woklab \
    "-_nearest:A0.5" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "snap rate short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=rate|stored=1|binding=snap_policy_approach_rate,snap_policy_approach_rate_override|value=0.5*}" != "${short_trace}" || {
    echo "not ok" 1 - "snap rate short value was not stored"
    exit 0
}
test "${short_trace#*LSXSNP1|*approach=0.5*}" != "${short_trace}" || {
    echo "not ok" 1 - "snap rate missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_SNAP_APPROACH_RATE=0.5" \
    -p 16 -Qkmeans -Woklab -_nearest \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "snap rate environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=rate|stored=1|binding=snap_policy_approach_rate,snap_policy_approach_rate_override|value=0.5*}" != "${env_trace}" || {
    echo "not ok" 1 - "snap rate environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_SNAP_APPROACH_RATE=2" \
    -p 16 -Qkmeans -_nearest "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "snap rate upper conversion failed"
    exit 0
}
test "${range_trace#*LSXSUB1|*key=rate|stored=1|binding=snap_policy_approach_rate,snap_policy_approach_rate_override|value=1*}" != "${range_trace}" || {
    echo "not ok" 1 - "snap rate upper endpoint was not clamped"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "snap rate short and environment output differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "snap rate image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "snap rate preserves image output"
exit 0
