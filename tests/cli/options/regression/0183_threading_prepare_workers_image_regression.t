#!/bin/sh
# Verify threading-policy prepare_workers through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_THREADING_POLICY|NULL|prepare_workers
# Registry binding: prepare_threads|prepare_threads_override
# Environment range: parse-signed-long

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0183-threading-prepare-workers-short-$$.six"
env_output="${artifact_dir}/0183-threading-prepare-workers-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -d fs \
    "-=balanced:B6:W2" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "threading prepare_workers short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=prepare_workers\|stored=1\|binding=prepare_threads,prepare_threads_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading prepare_workers short value was not stored"
    exit 0
}
case "${short_trace}" in *prepare_workers=2\ **) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading prepare_workers short did not cap the prepare workers"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -d fs \
    --env "SIXEL_THREADING_PREPARE_WORKERS=2" \
    "-=balanced:B6" "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "threading prepare_workers environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=prepare_workers\|stored=1\|binding=prepare_threads,prepare_threads_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading prepare_workers environment value was not stored"
    exit 0
}
case "${env_trace}" in *prepare_workers=2\ **) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading prepare_workers environment did not cap the prepare workers"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -d fs \
    --env "SIXEL_THREADING_PREPARE_WORKERS=-1" \
    "-=balanced:B6" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "threading prepare_workers out-of-range conversion failed"
    exit 0
}
case "${range_trace}" in *LSXSUB1\|*key=prepare_workers\|stored=1**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "threading prepare_workers accepted an out-of-range environment value"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "threading prepare_workers short and environment output differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "threading prepare_workers preserves effective image output"
exit 0
