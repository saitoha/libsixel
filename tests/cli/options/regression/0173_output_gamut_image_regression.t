#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
# Registry row: SIXEL_OPTION_SCHEMA_OUTPUT_COLORSPACE|NULL|gamut
# Registry binding: gamut
set -eux

echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 # SKIP converter disabled"
    exit 0
}
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/173-gamut-short-$$.six"
env_output="${artifact_dir}/173-gamut-env-$$.six"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qkmeans:seed=1 -dnone -aoff \
    -U gamma:Glocal-minde <"${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok 1 - gamut compact mapping conversion"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=gamut\|stored=1\|binding=gamut\|value=2*) true ;; *) false ;; esac || {
    echo "not ok 1 - gamut compact mapping binding"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qkmeans:seed=1 -dnone -aoff \
    --env "SIXEL_OUTPUT_COLORSPACE_GAMUT=local-minde" -U gamma \
    <"${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok 1 - gamut environment mapping conversion"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=gamut\|stored=1\|binding=gamut\|value=2*) true ;; *) false ;; esac || {
    echo "not ok 1 - gamut environment mapping binding"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok 1 - gamut short and environment image agreement"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    printf '# %s\n' "${lsqa_error}"
    echo "not ok 1 - gamut image sanity"
    exit 0
}
echo "ok 1 - gamut mapping suboption image"
exit 0
