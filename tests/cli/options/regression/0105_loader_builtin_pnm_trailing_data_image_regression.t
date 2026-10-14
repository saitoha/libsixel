#!/bin/sh
# Verify PNM trailing-data policy through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|g_loader_values + SIXEL_LOADER_INDEX_BUILTIN|pnm_trailing_data
# Registry binding: builtin_pnm_trailing_data

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/pnm-trailing-data-1x1.ppm"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0105-pnm_trailing_data-short-$$.six"
env_output="${artifact_dir}/0105-pnm_trailing_data-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Lbuiltin:N1!" -w16 -h16 "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "pnm_trailing_data short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=pnm_trailing_data\|stored=1\|binding=builtin_pnm_trailing_data\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "pnm_trailing_data short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_PNM_ALLOW_TRAILING_DATA=1" \
    "-Lbuiltin!" -w16 -h16 "${input_image}" \
    2>&1 >"${env_output}") || {
    echo "not ok" 1 - "pnm_trailing_data environment conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=pnm_trailing_data\|stored=1\|binding=builtin_pnm_trailing_data\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "pnm_trailing_data environment value was not stored"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Lbuiltin:N0!" -w16 -h16 "${input_image}" \
    >/dev/null && {
    echo "not ok" 1 - "pnm_trailing_data disabled control unexpectedly succeeded"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "pnm_trailing_data short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "pnm_trailing_data preserves image output"
exit 0
