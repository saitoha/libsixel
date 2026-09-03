#!/bin/sh
# Verify common loader osc11_timeout through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|NULL|osc11_timeout
# Registry binding: osc11_bg_query_timeout_ms
# Environment range: parse-signed-long

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
short_output="${artifact_dir}/0117-loader-osc11-timeout-short-$$.six"
env_output="${artifact_dir}/0117-loader-osc11-timeout-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_OSC11_BG_QUERY=0" \
    --env "SIXEL_LOADER_OSC11_BG_QUERY_TIMEOUT_MS=-1" \
    "-Lbuiltin:W0!" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "osc11_timeout short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=osc11_timeout|stored=1|binding=osc11_bg_query_timeout_ms|value=0*}" != "${short_trace}" || {
    echo "not ok" 1 - "osc11_timeout short value was not stored"
    exit 0
}

test "${short_trace#*LSXOSC1|*query=0|timeout_ms=0*}" != "${short_trace}" || {
    echo "not ok" 1 - "osc11_timeout short value did not reach query gate"
    exit 0
}

# The former strtol() path accepted signed zero as a zero timeout.
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_OSC11_BG_QUERY=0" \
    --env "SIXEL_LOADER_OSC11_BG_QUERY_TIMEOUT_MS=-0" \
    "-Lbuiltin!" "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "osc11_timeout environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=osc11_timeout|stored=1|binding=osc11_bg_query_timeout_ms|value=0*}" != "${env_trace}" || {
    echo "not ok" 1 - "osc11_timeout environment value was not stored"
    exit 0
}

test "${env_trace#*LSXOSC1|*query=0|timeout_ms=0*}" != "${env_trace}" || {
    echo "not ok" 1 - "osc11_timeout environment value did not reach query gate"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "osc11_timeout short and environment output differ"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_OSC11_BG_QUERY=0" \
    --env "SIXEL_LOADER_OSC11_BG_QUERY_TIMEOUT_MS=2147483648" \
    "-Lbuiltin!" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "osc11_timeout range conversion failed"
    exit 0
}

test "${range_trace#*LSXSUB1|*key=osc11_timeout|stored=1*}" = "${range_trace}" || {
    echo "not ok" 1 - "osc11_timeout accepted an out-of-range environment"
    exit 0
}

test "${range_trace#*LSXOSC1|*query=0|timeout_ms=50*}" != "${range_trace}" || {
    echo "not ok" 1 - "osc11_timeout did not retain its default"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "osc11_timeout image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "osc11_timeout preserves image output"
exit 0
