#!/bin/sh
# Verify builtin black point compensation through short/env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|NULL|cms_bpc
# Registry binding: cms_black_point_compensation
# Policy: docs/loader/color-management/README.md
# Image assertion: option-values-and-output-equivalence
set -eux
echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' "ok 1 - CMS image regression # SKIP img2sixel unavailable"
    exit 0
}


# The harness names the artifact directory but does not create it.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/tests/data/color-numeric/cli-bpc.png"
reference="${TOP_SRCDIR}/tests/data/color-numeric/cli-bpc-reference.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0129_loader_cms_bpc_image_regression-short-$$.six"
env_output="${artifact_dir}/0129_loader_cms_bpc_image_regression-env-$$.six"

short_trace=$(SIXEL_TRACE_TOPIC=suboption_contract,loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" --lookup-policy=none "-Lbuiltin:L1:Ebuiltin:Rrelative!!" \
    "$input" 2>&1 >"${short_output}") || {
    printf '\n%s\n' "not ok 1 - short CMS conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=cms_bpc\|stored=1\|binding=cms_black_point_compensation\|value=1*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - final short CMS setting is not builtin"
    exit 0
}

env_trace=$(SIXEL_TRACE_TOPIC=suboption_contract,loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" --lookup-policy=none --env "SIXEL_LOADER_CMS_BPC=1" "-Lbuiltin:Ebuiltin:Rrelative!!" \
    "$input" 2>&1 >"${env_output}") || {
    printf '\n%s\n' "not ok 1 - env CMS conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=cms_bpc\|stored=1\|binding=cms_black_point_compensation\|value=1*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - final env CMS setting is not builtin"
    exit 0
}

cmp -s "$short_output" "$env_output" || {
    printf '\n%s\n' "not ok 1 - short and environment CMS output differ"
    exit 0
}

test -s "${short_output}" || {
    printf '\n%s\n' 'not ok 1 - empty HDR output'
    exit 0
}

# Six neutral samples use an exact affine black-offset ICC reference. Full scanning
# avoids lookup-table approximation. The sample budget is 1.275 levels for
# SIXEL percentage rounding, 0.5 for decoding, 0.5 for the PNG oracle, and
# 0.025 for floating-point color arithmetic (2.3 RGB byte levels total).
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "$reference" "$short_output" 'builtin:Enone!' 2.3 || {
    printf '\n%s\n' "not ok 1 - independent managed RGB samples exceed 2.3 levels"
    exit 0
}

printf '\n%s\n' "ok 1 - Relative BPC reaches independently verified output"
exit 0
