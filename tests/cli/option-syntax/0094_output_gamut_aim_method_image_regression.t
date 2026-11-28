#!/bin/sh
# Verify output gamut aim_method through compact and environment paths.
# Policy: docs/encoder/output/gamut-mapping.md
# Registry row: SIXEL_OPTION_SCHEMA_OUTPUT_COLORSPACE|NULL|aim_method
# Registry binding: aim_method
# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

# Display P3 primaries outside sRGB give the gamut aim a nonzero offset.
input_image="${TOP_SRCDIR}/tests/data/color-numeric/cg-expanded-p3-binary.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0094-gamut-aim_method-short-$$.six"
env_output="${artifact_dir}/0094-gamut-aim_method-env-$$.six"
short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -U srgb:Daim:Mlut -dfs -p4 \
    "${input_image}" 2>&1 >"${short_output}") || {
    printf '\n%s\n' "not ok 1 - gamut aim_method compact conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=aim_method\|stored=1\|binding=aim_method\|value=1*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - gamut aim_method compact value was not stored"
    exit 0
}
case "${short_trace}" in *LSXDTH1\|*gamut_diffusion=1\|gamut_samples=0\|gamut_aim_method=1\|gamut_aim_offset=1*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - gamut aim_method compact value missed the dither"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_OUTPUT_COLORSPACE_AIM_METHOD=lut" -U srgb:Daim -dfs -p4 \
    "${input_image}" 2>&1 >"${env_output}") || {
    printf '\n%s\n' "not ok 1 - gamut aim_method environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=aim_method\|stored=1\|binding=aim_method\|value=1*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - gamut aim_method environment value was not stored"
    exit 0
}
case "${env_trace}" in *LSXDTH1\|*gamut_diffusion=1\|gamut_samples=0\|gamut_aim_method=1\|gamut_aim_offset=1*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - gamut aim_method environment value missed the dither"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    printf '\n%s\n' "not ok 1 - gamut aim_method compact and environment output differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Gamut target quality belongs to the dither target suite.
test -s "${short_output}" || {
    printf '\n%s\n' "not ok 1 - option output is empty"
    exit 0
}
printf '\n%s\n' "ok 1 - gamut aim_method preserves image output"
exit 0
