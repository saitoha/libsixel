#!/bin/sh
# Verify delta:refresh_spatial stores the same setting via short and environment paths.
# Policy: docs/encoder/output/delta-encoding.md
# Registry row: SIXEL_OPTION_SCHEMA_UPDATE_POLICY|g_update_policy_values + 1|refresh_spatial
# Registry binding: refresh_spatial|refresh_spatial_override

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

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "ok 1 # SKIP threading is disabled"
    exit 0
}

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
# Both files are compared below; create only their required private parent.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0028_update_delta_refresh_spatial_image_regression-short-$$.six"
env_output="${artifact_dir}/0028_update_delta_refresh_spatial_image_regression-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision-policy=8bit -Lbuiltin:cms_engine=none! \
    -dnone "-Zdelta:T48:J5:L4:A1:G1" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok 1 - delta:refresh_spatial short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=refresh_spatial\|stored=1\|binding=refresh_spatial,refresh_spatial_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok 1 - delta:refresh_spatial short value was not stored"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision-policy=8bit -Lbuiltin:cms_engine=none! \
    --env "SIXEL_UPDATE_DELTA_REFRESH_SPATIAL=1" -dnone -Z delta:T48:J5:L4:A1 \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok 1 - delta:refresh_spatial environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=refresh_spatial\|stored=1\|binding=refresh_spatial,refresh_spatial_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok 1 - delta:refresh_spatial environment value was not stored"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - delta:refresh_spatial output differs"
    exit 0
}
# This test owns exact option-route equivalence and consumer selection.
test -s "${short_output}" || {
    echo "not ok 1 - option-route output is empty"
    exit 0
}
echo "ok 1 - delta:refresh_spatial preserves image output"
exit 0
