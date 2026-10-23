#!/bin/sh
# Verify delta:residual stores the same setting via short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_UPDATE_POLICY|g_update_policy_values + 1|residual
# Registry binding: residual_enabled|residual_override

# Residual RGB241 observes an unmanaged RGB8 target. Applied CMS retains
# float samples even with --precision-policy=8bit.
# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0169_update_delta_residual_image_regression-short-$$.six"
env_output="${artifact_dir}/0169_update_delta_residual_image_regression-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision-policy=8bit -Lbuiltin:cms_engine=none! \
    -dnone "-Zdelta:T8:R1" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok 1 - delta:residual short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=residual\|stored=1\|binding=residual_enabled,residual_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok 1 - delta:residual short value was not stored"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision-policy=8bit -Lbuiltin:cms_engine=none! \
    --env "SIXEL_UPDATE_DELTA_RESIDUAL=1" -dnone -Z delta:T8 \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok 1 - delta:residual environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=residual\|stored=1\|binding=residual_enabled,residual_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok 1 - delta:residual environment value was not stored"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - delta:residual output differs"
    exit 0
}
# This test owns exact option-route equivalence and consumer selection.
test -s "${short_output}" || {
    echo "not ok 1 - option-route output is empty"
    exit 0
}
echo "ok 1 - delta:residual preserves image output"
exit 0
