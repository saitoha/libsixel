#!/bin/sh
# Verify output gamut samples through compact and environment paths.
# Policy: docs/encoder/output/gamut-mapping.md
# Registry row: SIXEL_OPTION_SCHEMA_OUTPUT_COLORSPACE|NULL|samples
# Registry binding: samples
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
short_output="${artifact_dir}/0093-gamut-samples-short-$$.six"
env_output="${artifact_dir}/0093-gamut-samples-env-$$.six"
short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -U srgb:Sshown -dfs -p4 \
    "${input_image}" 2>&1 >"${short_output}") || {
    printf '\n%s\n' "not ok 1 - gamut samples compact conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=samples\|stored=1\|binding=samples\|value=1*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - gamut samples compact value was not stored"
    exit 0
}
case "${short_trace}" in *LSXDTH1\|*gamut_diffusion=0\|gamut_samples=1\|gamut_aim_method=0\|gamut_aim_offset=0*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - gamut samples compact value missed the dither"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_OUTPUT_COLORSPACE_SAMPLES=shown" -U srgb -dfs -p4 \
    "${input_image}" 2>&1 >"${env_output}") || {
    printf '\n%s\n' "not ok 1 - gamut samples environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=samples\|stored=1\|binding=samples\|value=1*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - gamut samples environment value was not stored"
    exit 0
}
case "${env_trace}" in *LSXDTH1\|*gamut_diffusion=0\|gamut_samples=1\|gamut_aim_method=0\|gamut_aim_offset=0*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - gamut samples environment value missed the dither"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    printf '\n%s\n' "not ok 1 - gamut samples compact and environment output differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Gamut target quality belongs to the dither target suite.
test -s "${short_output}" || {
    printf '\n%s\n' "not ok 1 - option output is empty"
    exit 0
}
printf '\n%s\n' "ok 1 - gamut samples preserves image output"
exit 0
