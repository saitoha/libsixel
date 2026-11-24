#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Registry row: SIXEL_OPTION_SCHEMA_SAMPLING_POLICY|NULL|solid
# Registry binding: palette_sample_solid|palette_sample_solid_override
# Image assertion: option-values-and-output-equivalence

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo '1..0 # SKIP img2sixel is disabled in this build'
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/sampling-solid-short-$$.six"
env_output="${artifact_dir}/sampling-solid-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qheckbert -d none \
    --threading-policy=greedy:global_budget=1 -4 "auto:Soff" \
    "${input}" 2>&1 >"${short_output}") || {
    printf '\n%s\n' 'not ok 1 - short solid option conversion failed'
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=solid\|stored=1\|binding=palette_sample_solid,palette_sample_solid_override**) true ;; *) false ;; esac || {
    printf '\n%s\n' 'not ok 1 - short solid option was not stored'
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qheckbert -d none \
    --threading-policy=greedy:global_budget=1 \
    --env "SIXEL_SAMPLING_SOLID=off" -4 "auto" \
    "${input}" 2>&1 >"${env_output}") || {
    printf '\n%s\n' 'not ok 1 - solid environment conversion failed'
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=solid\|stored=1\|binding=palette_sample_solid,palette_sample_solid_override**) true ;; *) false ;; esac || {
    printf '\n%s\n' 'not ok 1 - solid environment value was not stored'
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    printf '\n%s\n' 'not ok 1 - short and environment solid output differ'
    exit 0
}
test -s "${short_output}" || {
    printf '\n%s\n' 'not ok 1 - solid option output is empty'
    exit 0
}
printf '\n%s\n' 'ok 1 - short and environment solid values preserve image output'
exit 0
