#!/bin/sh
# Verify cover snap timing through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_COVER_POLICY|NULL|snap_timing
# Registry binding: cover_policy_snap_timing|cover_policy_snap_timing_override
# Palette contract: timing=all

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
short_output="${artifact_dir}/0137-snap-timing-short-$$.six"
env_output="${artifact_dir}/0137-snap-timing-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qkmeans -6 -Woklab \
    "-aauto:Iall" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "snap timing short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=snap_timing|stored=1|binding=cover_policy_snap_timing,cover_policy_snap_timing_override|value=4*}" != "${short_trace}" || {
    echo "not ok" 1 - "snap timing short value was not stored"
    exit 0
}
test "${short_trace#*LSXSNP1|*timing=all*}" != "${short_trace}" || {
    echo "not ok" 1 - "snap timing missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_SNAP_TIMING_POLICY=all" \
    -p 16 -Qkmeans -6 -Woklab -aauto \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "snap timing environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=snap_timing|stored=1|binding=cover_policy_snap_timing,cover_policy_snap_timing_override|value=4*}" != "${env_trace}" || {
    echo "not ok" 1 - "snap timing environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "snap timing short and environment output differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "snap timing image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "cover snap timing preserves image output"
exit 0
