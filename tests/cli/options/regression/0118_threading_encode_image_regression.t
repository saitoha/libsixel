#!/bin/sh
# Verify threading-policy encode through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_THREADING_POLICY|NULL|encode
# Registry binding: encode_threads|encode_threads_override
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
short_output="${artifact_dir}/0118-threading-encode-short-$$.six"
env_output="${artifact_dir}/0118-threading-encode-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs \
    "-=balanced:B6:E3" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "threading encode short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=encode\|stored=1\|binding=encode_threads,encode_threads_override\|value=3**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading encode short value was not stored"
    exit 0
}
case "${short_trace}" in *LSXDTH1\|*dither_threads=3\|encode_threads=3**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading encode short did not reserve the encode workers"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs \
    --env "SIXEL_THREADING_ENCODE=3" \
    "-=balanced:B6" "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "threading encode environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=encode\|stored=1\|binding=encode_threads,encode_threads_override\|value=3**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading encode environment value was not stored"
    exit 0
}
case "${env_trace}" in *LSXDTH1\|*dither_threads=3\|encode_threads=3**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "threading encode environment did not reserve the encode workers"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs \
    --env "SIXEL_THREADING_ENCODE=-1" \
    "-=balanced:B6" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "threading encode out-of-range conversion failed"
    exit 0
}
case "${range_trace}" in *LSXSUB1\|*key=encode\|stored=1**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "threading encode accepted an out-of-range environment value"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "threading encode short and environment output differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "threading encode preserves effective image output"
exit 0
