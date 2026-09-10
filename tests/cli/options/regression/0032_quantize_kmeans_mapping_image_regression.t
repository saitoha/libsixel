#!/bin/sh
# Verify kmeans:mapping preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS|mapping
# Registry binding: quantize_model_kmeans_mapping_mode|quantize_model_kmeans_mapping_override

set -eux

# OpenBSD rand() is non-deterministic by default, so pin the unrelated
# Kmeans initializer while comparing independent converter processes.
SIXEL_PALETTE_KMEANS_SEED=1
export SIXEL_PALETTE_KMEANS_SEED

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0032-quantize-kmeans-mapping-short-$$.six"
env_output="${artifact_dir}/0032-quantize-kmeans-mapping-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qkmeans:Msrgb" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "kmeans:mapping short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=mapping|stored=1|binding=quantize_model_kmeans_mapping_mode,quantize_model_kmeans_mapping_override*}" != "${short_trace}" || {
    echo "not ok" 1 - "mapping short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEANS_MAPPING=srgb" -p 16 "-Qkmeans" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "kmeans:mapping env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=mapping|stored=1|binding=quantize_model_kmeans_mapping_mode,quantize_model_kmeans_mapping_override*}" != "${env_trace}" || {
    echo "not ok" 1 - "mapping environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "kmeans:mapping short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "kmeans:mapping image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "kmeans:mapping preserves image output"
exit 0
