#!/bin/sh
# Verify delta:refresh_ms stores the same setting via short and environment paths.
# Policy: docs/encoder/output/delta-encoding.md
# Registry row: SIXEL_OPTION_SCHEMA_UPDATE_POLICY|g_update_policy_values + 1|refresh_ms
# Registry binding: refresh_ms|refresh_ms_override

# Periodic refresh observes an unmanaged RGB8 target. Applied CMS retains
# float samples even with --precision-policy=8bit.
# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# The registry harness provides a path; this test owns its lazy creation.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0188_update_delta_refresh_ms_image_regression-short-$$.six"
env_output="${artifact_dir}/0188_update_delta_refresh_ms_image_regression-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision-policy=8bit -Lbuiltin:cms_engine=none! \
    -dnone "-Zdelta:T8:J80" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok 1 - delta:refresh_ms short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=refresh_ms\|stored=1\|binding=refresh_ms,refresh_ms_override\|value=80**) true ;; *) false ;; esac || {
    echo "not ok 1 - delta:refresh_ms short value was not stored"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision-policy=8bit -Lbuiltin:cms_engine=none! \
    --env "SIXEL_UPDATE_DELTA_REFRESH_MS=80" -dnone -Z delta:T8 \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok 1 - delta:refresh_ms environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=refresh_ms\|stored=1\|binding=refresh_ms,refresh_ms_override\|value=80**) true ;; *) false ;; esac || {
    echo "not ok 1 - delta:refresh_ms environment value was not stored"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - delta:refresh_ms output differs"
    exit 0
}
# This test owns exact option-route equivalence and consumer selection.
test -s "${short_output}" || {
    echo "not ok 1 - option-route output is empty"
    exit 0
}
echo "ok 1 - delta:refresh_ms preserves image output"
exit 0
