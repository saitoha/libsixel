#!/bin/sh
# Verify the cover snap target through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_COVER_POLICY|NULL|snap_target
# Registry binding: cover_policy_snap_target|cover_policy_snap_target_override
# Palette contract: target=reversible

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
reference_image="${input_image}"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0024-snap-target-short-$$.six"
env_output="${artifact_dir}/0024-snap-target-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qkmeans -6 -Woklab \
    "-aauto:Treversible" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "snap target short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=snap_target|stored=1|binding=cover_policy_snap_target,cover_policy_snap_target_override|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "snap target short value was not stored"
    exit 0
}
test "${short_trace#*LSXSNP1|*target=reversible*}" != "${short_trace}" || {
    echo "not ok" 1 - "snap target missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_SNAP_TARGET_POLICY=reversible" \
    -p 16 -Qkmeans -6 -Woklab -aauto \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "snap target environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=snap_target|stored=1|binding=cover_policy_snap_target,cover_policy_snap_target_override|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "snap target environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "snap target short and environment output differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "snap target image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "cover snap target preserves image output"
exit 0
