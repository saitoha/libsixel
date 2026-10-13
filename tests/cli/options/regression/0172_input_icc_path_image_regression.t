#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Registry row: SIXEL_OPTION_SCHEMA_INPUT_COLORSPACE|g_input_color_values + 4|path
# Registry binding: path|path_override
set -eux

echo "1..1"
set -v

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "ok 1 # SKIP converter disabled"
    exit 0
}
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/172-icc-short-$$.png"
env_output="${artifact_dir}/172-icc-env-$$.png"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
profile="${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    -T "icc:P${profile}" <"${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok 1 - ICC short path conversion"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=path\|stored=1\|binding=path,path_override\|value="${profile}"**) true ;; *) false ;; esac || {
    echo "not ok 1 - ICC short path binding"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env "SIXEL_DECODER_INPUT_COLORSPACE_ICC_PATH=${profile}" -T icc \
    <"${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok 1 - ICC environment path conversion"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=path\|stored=1\|binding=path,path_override\|value="${profile}"**) true ;; *) false ;; esac || {
    echo "not ok 1 - ICC environment path binding"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - ICC short and environment image agreement"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -Rdisplay-p3 \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    printf '# %s\n' "${lsqa_error}"
    echo "not ok 1 - ICC image sanity"
    exit 0
}
echo "ok 1 - ICC file suboption image"
exit 0
