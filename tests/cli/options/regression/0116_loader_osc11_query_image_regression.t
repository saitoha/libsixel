#!/bin/sh
# Verify common loader osc11_query through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|NULL|osc11_query
# Registry binding: osc11_bg_query
# Policy: docs/loader/alpha-policy.md
# Policy: docs/loader/background-policy.md

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
short_output="${artifact_dir}/0116-loader-osc11-query-short-$$.six"
env_output="${artifact_dir}/0116-loader-osc11-query-env-$$.six"

unset SIXEL_LOADER_OSC11_BG_QUERY
default_trace=$(set +xv; SIXEL_TRACE_TOPIC=loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "-Lbuiltin!" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "osc11_query frontend default conversion failed"
    exit 0
}

test "${default_trace#*LSXOSC1|enabled=1|has_bgcolor=0|stdout_tty=0|stderr_tty=0|alpha_policy=1|query=0*}" != "${default_trace}" || {
    echo "not ok" 1 - "auto alpha policy did not suppress OSC11 query"
    exit 0
}

empty_trace=$(set +xv; SIXEL_TRACE_TOPIC=loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_OSC11_BG_QUERY=" "-Lbuiltin!" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "empty osc11_query conversion failed"
    exit 0
}

test "${empty_trace#*LSXOSC1|enabled=0|has_bgcolor=0|stdout_tty=0|stderr_tty=0|alpha_policy=1|query=0*}" != "${empty_trace}" || {
    echo "not ok" 1 - "empty osc11_query no longer suppresses the frontend default"
    exit 0
}

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_OSC11_BG_QUERY=1" \
    "-Lbuiltin:Q0!" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "osc11_query short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=osc11_query|stored=1|binding=osc11_bg_query|value=0*}" != "${short_trace}" || {
    echo "not ok" 1 - "osc11_query short value was not stored"
    exit 0
}

test "${short_trace#*LSXOSC1|enabled=0|has_bgcolor=0|stdout_tty=0|stderr_tty=0|alpha_policy=1|query=0*}" != "${short_trace}" || {
    echo "not ok" 1 - "osc11_query short value did not reach query gate"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_OSC11_BG_QUERY=0" "-Lbuiltin!" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "osc11_query environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=osc11_query|stored=1|binding=osc11_bg_query|value=0*}" != "${env_trace}" || {
    echo "not ok" 1 - "osc11_query environment value was not stored"
    exit 0
}

test "${env_trace#*LSXOSC1|enabled=0|has_bgcolor=0|stdout_tty=0|stderr_tty=0|alpha_policy=1|query=0*}" != "${env_trace}" || {
    echo "not ok" 1 - "osc11_query environment value did not reach query gate"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "osc11_query short and environment output differ"
    exit 0
}

control_trace=$(set +xv; SIXEL_TRACE_TOPIC=loader \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_OSC11_BG_QUERY=0" \
    --alpha-policy=composite \
    "-Lbuiltin:Q1!" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "osc11_query control conversion failed"
    exit 0
}

test "${control_trace#*LSXOSC1|enabled=1|has_bgcolor=0|stdout_tty=0|stderr_tty=0|alpha_policy=0|query=0*}" != "${control_trace}" || {
    echo "not ok" 1 - "osc11_query enabled value did not reach query gate"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "osc11_query image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "osc11_query preserves image output"
exit 0
