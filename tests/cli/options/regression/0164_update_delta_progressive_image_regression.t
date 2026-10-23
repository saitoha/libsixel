#!/bin/sh
# Verify progressive selection reaches the real still presentation consumer.
# Registry row: SIXEL_OPTION_SCHEMA_UPDATE_POLICY|g_update_policy_values + 1|progressive
# Registry binding: progressive_enabled|progressive_override
# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

# Captures below are compared; create their per-test directory lazily.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0171_progressive_short-$$.six"
env_output="${artifact_dir}/0171_progressive_env-$$.six"
short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Z delta:P1 \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok 1 - short progressive conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=progressive\|stored=1\|binding=progressive_enabled,progressive_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok 1 - short progressive assignment missing"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_UPDATE_DELTA_PROGRESSIVE=1" -Z delta \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok 1 - environment progressive conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=progressive\|stored=1\|binding=progressive_enabled,progressive_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok 1 - environment progressive assignment missing"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - progressive output differs"
    exit 0
}
# This test owns exact option-route equivalence and consumer selection.
test -s "${short_output}" || {
    echo "not ok 1 - option-route output is empty"
    exit 0
}
echo "ok 1 - progressive image output and option routes agree"
exit 0
