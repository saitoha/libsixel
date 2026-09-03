#!/bin/sh
# Verify fhedt:tile_xy through short, canonical, and legacy env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LUT_POLICY|g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT|tile_xy
# Registry binding: lut_policy_fhedt_tile_xy|lut_policy_fhedt_tile_xy_override
# Lookup contract: precision=8bit|tile_xy=5

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
short_output="${artifact_dir}/0133-fhedt-tile-xy-short-$$.six"
env_output="${artifact_dir}/0133-fhedt-tile-xy-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    "-~fhedt:X5" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "fhedt:tile_xy short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=tile_xy|stored=1|binding=lut_policy_fhedt_tile_xy,lut_policy_fhedt_tile_xy_override|value=5*}" != "${short_trace}" || {
    echo "not ok" 1 - "fhedt tile_xy short value was not stored"
    exit 0
}
test "${short_trace#*LSXFHD1|*precision=8bit|tile_xy=5*}" != \
    "${short_trace}" || {
    echo "not ok" 1 - "fhedt tile_xy did not reach the backend"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env "SIXEL_LOOKUP_FHEDT_TILE_XY=5" "-~fhedt" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "fhedt:tile_xy environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=tile_xy|stored=1|binding=lut_policy_fhedt_tile_xy,lut_policy_fhedt_tile_xy_override|value=5*}" != "${env_trace}" || {
    echo "not ok" 1 - "fhedt tile_xy environment value was not stored"
    exit 0
}
test "${env_trace#*LSXFHD1|*precision=8bit|tile_xy=5*}" != "${env_trace}" || {
    echo "not ok" 1 - "environment tile_xy missed the backend"
    exit 0
}
float_trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --precision=float32 "-~fhedt:X5" "${input_image}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "fhedt tile_xy float32 conversion failed"
    exit 0
}
test "${float_trace#*LSXFHD1|*precision=float32|tile_xy=5*}" != \
    "${float_trace}" || {
    echo "not ok" 1 - "fhedt tile_xy missed the float32 backend"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "fhedt tile_xy outputs differ"
    exit 0
}

legacy_trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env "SIXEL_FHEDT_TILE_XY=5" "-~fhedt" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "legacy tile_xy conversion failed"
    exit 0
}
test "${legacy_trace#*LSXFHD1|*tile_xy=5*}" != "${legacy_trace}" || {
    echo "not ok" 1 - "legacy tile_xy was not consumed"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "fhedt tile_xy image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "fhedt:tile_xy preserves effective image output"
exit 0
