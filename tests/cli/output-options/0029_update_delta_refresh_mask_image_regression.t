#!/bin/sh
# Verify delta:refresh_mask stores the same setting via short and environment paths.
# Policy: docs/encoder/output/delta-encoding.md
# Registry row: SIXEL_OPTION_SCHEMA_UPDATE_POLICY|g_update_policy_values + 1|refresh_mask
# Registry binding: refresh_mask|refresh_mask_override

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


# The compared SIXEL captures require the harness-owned local directory.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0029_update_delta_refresh_mask_image_regression-short-$$.six"
env_output="${artifact_dir}/0029_update_delta_refresh_mask_image_regression-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision-policy=8bit -Lbuiltin:cms_engine=none! \
    -dnone "-Zdelta:T48:J5:Bhorizontal8" "${input_image}" 2>&1 >"${short_output}") || {
    printf '\n%s\n' "not ok 1 - delta:refresh_mask short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=refresh_mask\|stored=1\|binding=refresh_mask,refresh_mask_override\|value=1**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - delta:refresh_mask short value was not stored"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision-policy=8bit -Lbuiltin:cms_engine=none! \
    --env "SIXEL_UPDATE_DELTA_REFRESH_MASK=horizontal8" -dnone -Z delta:T48:J5 \
    "${input_image}" 2>&1 >"${env_output}") || {
    printf '\n%s\n' "not ok 1 - delta:refresh_mask environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=refresh_mask\|stored=1\|binding=refresh_mask,refresh_mask_override\|value=1**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - delta:refresh_mask environment value was not stored"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    printf '\n%s\n' "not ok 1 - delta:refresh_mask output differs"
    exit 0
}
# This test owns exact option-route equivalence and consumer selection.
test -s "${short_output}" || {
    printf '\n%s\n' "not ok 1 - option-route output is empty"
    exit 0
}
printf '\n%s\n' "ok 1 - delta:refresh_mask preserves image output"
exit 0
