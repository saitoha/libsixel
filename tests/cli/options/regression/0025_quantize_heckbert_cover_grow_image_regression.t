#!/bin/sh
# Verify heckbert:cover_grow preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|NULL|cover_grow
# Registry binding: quantize_model_cover_grow|quantize_model_cover_grow_override

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0025-quantize-heckbert-cover_grow-short-$$.six"
env_output="${artifact_dir}/0025-quantize-heckbert-cover_grow-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qheckbert:Ccorners:V1" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "heckbert:cover_grow short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=cover_grow|stored=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "cover_grow short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_COVER_GROW=1" -p 16 "-Qheckbert:Ccorners" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "heckbert:cover_grow env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=cover_grow|stored=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "cover_grow environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "heckbert:cover_grow short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "heckbert:cover_grow image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "heckbert:cover_grow preserves image output"
exit 0
