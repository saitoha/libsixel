#!/bin/sh
# Policy: docs/functionality/snap-policy.md
# Top-level option regression
# Verify the snap policy through short and environment paths.
# Palette contract: policy=reversible

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
reference_image="${input_image}"
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
test "${short_trace#*LSXSNP1|*policy=reversible*}" != "${short_trace}" || {
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
test "${env_trace#*LSXSNP1|*policy=reversible*}" != "${env_trace}" || {
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
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "snap policy image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "snap policy preserves image output"
exit 0
