#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Registry row: SIXEL_OPTION_SCHEMA_OUTPUT_COLORSPACE|g_output_color_values + 7|bpc
# Registry binding: bpc
# Image assertion: option-values-and-output-equivalence
set -eux

echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 # SKIP converter disabled"
    exit 0
}
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0130-icc-bpc-short-$$.six"
env_output="${artifact_dir}/0130-icc-bpc-env-$$.six"
input_image="${TOP_SRCDIR}/tests/data/color-numeric/cli-bpc.png"
profile="${TOP_SRCDIR}/tests/data/color-numeric/output-bpc.icc"

export SIXEL_LOADER_CMS_BPC=0 SIXEL_LOADER_PREFER_8BIT=0

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qkmeans:seed=1 -dnone -aoff -_none \
    -Lbuiltin:Ebuiltin:Rrelative!! -U "icc:P${profile}:B1" <"${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok 1 - ICC compact BPC conversion"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=bpc\|stored=1\|binding=bpc\|value=1*) true ;; *) false ;; esac || {
    echo "not ok 1 - ICC compact BPC binding"
    exit 0
}
# Keep compensated output byte-identical across both option routes.
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qkmeans:seed=1 -dnone -aoff -_none \
    -Lbuiltin:Ebuiltin:Rrelative!! \
    --env "SIXEL_OUTPUT_COLORSPACE_ICC_BPC=1" -U "icc:P${profile}" \
    <"${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok 1 - ICC environment BPC conversion"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=bpc\|stored=1\|binding=bpc\|value=1*) true ;; *) false ;; esac || {
    echo "not ok 1 - ICC environment BPC binding"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - ICC short and environment image agreement"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}
echo "ok 1 - ICC BPC suboption image"
exit 0
