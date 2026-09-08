#!/bin/sh
# Verify PNM truncated-ASCII policy through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_BUILTIN|pnm_truncated_ascii
# Registry binding: builtin_pnm_truncated_ascii

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/pnm-truncated-ascii-2x1.ppm"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0106-pnm_truncated_ascii-short-$$.six"
env_output="${artifact_dir}/0106-pnm_truncated_ascii-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Lbuiltin:Y1!" "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "pnm_truncated_ascii short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=pnm_truncated_ascii|stored=1|binding=builtin_pnm_truncated_ascii|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "pnm_truncated_ascii short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_PNM_ALLOW_TRUNCATED_ASCII=1" \
    "-Lbuiltin!" "${input_image}" \
    2>&1 >"${env_output}") || {
    echo "not ok" 1 - "pnm_truncated_ascii environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=pnm_truncated_ascii|stored=1|binding=builtin_pnm_truncated_ascii|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "pnm_truncated_ascii environment value was not stored"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Lbuiltin:Y0!" "${input_image}" \
    >/dev/null && {
    echo "not ok" 1 - "pnm_truncated_ascii disabled control unexpectedly succeeded"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "pnm_truncated_ascii short and env output differ"
    exit 0
}

# This registry-path test compares exact encoder output. Avoid resampling a
# two-pixel malformed fixture because resampler rounding is toolchain-specific.
echo "ok" 1 - "pnm_truncated_ascii preserves exact image output"
exit 0
