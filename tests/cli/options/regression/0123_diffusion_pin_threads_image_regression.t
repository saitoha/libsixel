#!/bin/sh
# Verify common diffusion pin_threads through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|NULL|pin_threads
# Registry binding: dither_pin_threads|dither_pin_threads_override
# Dither contract: diffuse=fs|*pin_threads=0|pin_threads_override=1

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP threading is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
reference_image="${input_image}"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0123-diffusion-pin-threads-short-$$.six"
env_output="${artifact_dir}/0123-diffusion-pin-threads-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    -d "fs:I0" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "pin_threads short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1\|*key=pin_threads\|stored=1\|binding=dither_pin_threads,dither_pin_threads_override\|value=0*}" != "${short_trace}" || {
    echo "not ok" 1 - "pin_threads short value was not stored"
    exit 0
}

test "${short_trace#*LSXDTH1\|*diffuse=fs\|*pin_threads=0\|pin_threads_override=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "pin_threads short value was not effective"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    --env "SIXEL_DITHER_PIN_THREADS=0" -d "fs" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "pin_threads environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1\|*key=pin_threads\|stored=1\|binding=dither_pin_threads,dither_pin_threads_override\|value=0*}" != "${env_trace}" || {
    echo "not ok" 1 - "pin_threads environment value was not stored"
    exit 0
}

test "${env_trace#*LSXDTH1\|*diffuse=fs\|*pin_threads=0\|pin_threads_override=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "pin_threads environment value was not effective"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "pin_threads short and environment output differ"
    exit 0
}

invalid_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    --env "SIXEL_DITHER_PIN_THREADS=off" -d "fs" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "invalid pin_threads conversion failed"
    exit 0
}

test "${invalid_trace#*LSXSUB1\|*key=pin_threads\|stored=1*}" = \
    "${invalid_trace}" || {
    echo "not ok" 1 - "pin_threads accepted a nonnumeric boolean"
    exit 0
}

test "${invalid_trace#*LSXDTH1\|*pin_threads=1\|pin_threads_override=0*}" != "${invalid_trace}" || {
    echo "not ok" 1 - "invalid pin_threads changed the default"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "pin_threads image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "pin_threads preserves effective image output"
exit 0
