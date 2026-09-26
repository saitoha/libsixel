#!/bin/sh
# Verify clamp threshold through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_CLAMP_POLICY|NULL|threshold
# Registry binding: clamp_threshold|clamp_threshold_override
# Dither contract: clamp_effective=1|clamp_threshold=1.05
# Policy: docs/encoder/palette-application/clamp-policy.md
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
short_output="${artifact_dir}/0168-clamp-short-$$.six"
env_output="${artifact_dir}/0168-clamp-env-$$.six"
short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -0soft:T1.05 -dfs -=1 \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok 1 - clamp short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=threshold\|stored=1\|binding=clamp_threshold,clamp_threshold_override\|value=1.05*) true ;; *) false ;; esac || {
    echo "not ok 1 - clamp short threshold was not stored"
    exit 0
}
case "${short_trace}" in *LSXDTH1\|*clamp_effective=1\|clamp_threshold=1.05*) true ;; *) false ;; esac || {
    echo "not ok 1 - clamp threshold missed the dither"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_CLAMP_THRESHOLD=1.05" -0soft -dfs -=1 \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok 1 - clamp environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=threshold\|stored=1\|binding=clamp_threshold,clamp_threshold_override\|value=1.05*) true ;; *) false ;; esac || {
    echo "not ok 1 - clamp environment threshold was not stored"
    exit 0
}
case "${env_trace}" in *LSXDTH1\|*clamp_effective=1\|clamp_threshold=1.05*) true ;; *) false ;; esac || {
    echo "not ok 1 - environment threshold missed the dither"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - clamp short and environment output differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b MS-SSIM:0.98 "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok 1 - clamp image quality regressed"
    printf '# %s\n' "${lsqa_error}"
    exit 0
}
echo "ok 1 - clamp threshold preserves image output"
exit 0
