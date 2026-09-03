#!/bin/sh
# Verify fhedt:pin_threads through short, canonical, and legacy env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LUT_POLICY|g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT|pin_threads
# Registry binding: lut_policy_fhedt_pin_threads|lut_policy_fhedt_pin_threads_override
# Lookup contract: precision=8bit|*pin_threads=1

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
short_output="${artifact_dir}/0128-fhedt-pin-threads-short-$$.six"
env_output="${artifact_dir}/0128-fhedt-pin-threads-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    "-~fhedt:P1" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "fhedt:pin_threads short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=pin_threads|stored=1|binding=lut_policy_fhedt_pin_threads,lut_policy_fhedt_pin_threads_override|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "fhedt pin_threads short value was not stored"
    exit 0
}
test "${short_trace#*LSXFHD1|*precision=8bit|*pin_threads=1*}" != \
    "${short_trace}" || {
    echo "not ok" 1 - "fhedt pin_threads did not reach the backend"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env "SIXEL_LOOKUP_FHEDT_PIN_THREADS=1" "-~fhedt" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "fhedt:pin_threads environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=pin_threads|stored=1|binding=lut_policy_fhedt_pin_threads,lut_policy_fhedt_pin_threads_override|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "fhedt pin_threads environment value was not stored"
    exit 0
}
test "${env_trace#*LSXFHD1|*precision=8bit|*pin_threads=1*}" != \
    "${env_trace}" || {
    echo "not ok" 1 - "environment pin_threads missed the backend"
    exit 0
}
float_trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --precision=float32 "-~fhedt:P1" "${input_image}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "fhedt pin_threads float32 conversion failed"
    exit 0
}
test "${float_trace#*LSXFHD1|*precision=float32|*pin_threads=1*}" != \
    "${float_trace}" || {
    echo "not ok" 1 - "fhedt pin_threads missed the float32 backend"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "fhedt pin_threads outputs differ"
    exit 0
}

legacy_trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env "SIXEL_FHEDT_PIN_THREADS=1" "-~fhedt" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "legacy pin_threads conversion failed"
    exit 0
}
test "${legacy_trace#*LSXFHD1|*pin_threads=1*}" != "${legacy_trace}" || {
    echo "not ok" 1 - "legacy pin_threads was not consumed"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "fhedt pin_threads image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "fhedt:pin_threads preserves effective image output"
exit 0
