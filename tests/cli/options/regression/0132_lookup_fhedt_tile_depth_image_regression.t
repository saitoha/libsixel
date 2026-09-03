#!/bin/sh
# Verify fhedt:tile_depth through short, canonical, and legacy env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LUT_POLICY|g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT|tile_depth
# Registry binding: lut_policy_fhedt_tile_depth|lut_policy_fhedt_tile_depth_override
# Lookup contract: precision=8bit|*tile_depth=6

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0132-fhedt-tile-depth-short-$$.six"
env_output="${artifact_dir}/0132-fhedt-tile-depth-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    "-~fhedt:T6" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "fhedt:tile_depth short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=tile_depth|stored=1|binding=lut_policy_fhedt_tile_depth,lut_policy_fhedt_tile_depth_override|value=6*}" != "${short_trace}" || {
    echo "not ok" 1 - "fhedt tile_depth short value was not stored"
    exit 0
}
test "${short_trace#*LSXFHD1|*precision=8bit|*tile_depth=6*}" != \
    "${short_trace}" || {
    echo "not ok" 1 - "fhedt tile_depth did not reach the backend"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env "SIXEL_LOOKUP_FHEDT_TILE_DEPTH=6" "-~fhedt" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "fhedt:tile_depth environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=tile_depth|stored=1|binding=lut_policy_fhedt_tile_depth,lut_policy_fhedt_tile_depth_override|value=6*}" != "${env_trace}" || {
    echo "not ok" 1 - "fhedt tile_depth environment value was not stored"
    exit 0
}
test "${env_trace#*LSXFHD1|*precision=8bit|*tile_depth=6*}" != \
    "${env_trace}" || {
    echo "not ok" 1 - "environment tile_depth missed the backend"
    exit 0
}
float_trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --precision=float32 "-~fhedt:T6" "${input_image}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "fhedt tile_depth float32 conversion failed"
    exit 0
}
test "${float_trace#*LSXFHD1|*precision=float32|*tile_depth=6*}" != \
    "${float_trace}" || {
    echo "not ok" 1 - "fhedt tile_depth missed the float32 backend"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "fhedt tile_depth outputs differ"
    exit 0
}

legacy_trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env "SIXEL_FHEDT_TILE_DEPTH=6" "-~fhedt" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "legacy tile_depth conversion failed"
    exit 0
}
test "${legacy_trace#*LSXFHD1|*tile_depth=6*}" != "${legacy_trace}" || {
    echo "not ok" 1 - "legacy tile_depth was not consumed"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "fhedt tile_depth image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "fhedt:tile_depth preserves effective image output"
exit 0
