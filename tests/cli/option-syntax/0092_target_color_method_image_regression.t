#!/bin/sh
# Verify target color method through short and environment paths.
# Policy: docs/encoder/palette-application/target-color-policy.md
# Registry row: SIXEL_OPTION_SCHEMA_TARGET_COLOR_POLICY|NULL|method
# Registry binding: target_color_method|target_color_method_override
# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

# Display P3 primaries outside sRGB give the -U target a nonzero offset.
input_image="${TOP_SRCDIR}/tests/data/color-numeric/cg-expanded-p3-binary.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0092-target-method-short-$$.six"
env_output="${artifact_dir}/0092-target-method-env-$$.six"
short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Yfull:Mlut -dfs -p4 \
    "${input_image}" 2>&1 >"${short_output}") || {
    printf '\n%s\n' "not ok 1 - target method short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=method\|stored=1\|binding=target_color_method,target_color_method_override\|value=1*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - target method short value was not stored"
    exit 0
}
case "${short_trace}" in *LSXDTH1\|*target_policy=2\|target_method=1\|target_lut_size=33\|target_tolerance=0.001\|target_offset=1*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - target method short value missed the dither"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_TARGET_COLOR_METHOD=lut" -Yfull -dfs -p4 \
    "${input_image}" 2>&1 >"${env_output}") || {
    printf '\n%s\n' "not ok 1 - target method environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=method\|stored=1\|binding=target_color_method,target_color_method_override\|value=1*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - target method environment value was not stored"
    exit 0
}
case "${env_trace}" in *LSXDTH1\|*target_policy=2\|target_method=1\|target_lut_size=33\|target_tolerance=0.001\|target_offset=1*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - target method environment value missed the dither"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    printf '\n%s\n' "not ok 1 - target method short and environment output differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Target color quality belongs to the dither target suite.
test -s "${short_output}" || {
    printf '\n%s\n' "not ok 1 - option output is empty"
    exit 0
}
printf '\n%s\n' "ok 1 - target method preserves image output"
exit 0
