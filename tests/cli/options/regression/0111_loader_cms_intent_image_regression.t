#!/bin/sh
# Verify common loader cms_intent through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|NULL|cms_intent
# Registry binding: cms_rendering_intent_order

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}
test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_embedded_srgb_icc_vp8_static_12x8.webp"
reference_image="${input_image}"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0111-loader-cms-intent-short-$$.six"
env_output="${artifact_dir}/0111-loader-cms-intent-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_CMS_RENDERING_INTENT=invalid" \
    --env "SIXEL_CMS_RENDERING_INTENT=perceptual!" \
    --cms-engine=builtin "-Llibpng:Rrelative+saturation!" \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "cms_intent short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=cms_intent|stored=1|binding=cms_rendering_intent_order|value=relative+saturation!*}" != "${short_trace}" || {
    echo "not ok" 1 - "cms_intent short value was not stored"
    exit 0
}

test "${short_trace#*LSXCMS1|intent_order=relative+saturation|exclusive=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "cms_intent short value did not reach CMS"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_CMS_RENDERING_INTENT=relative,saturation!" \
    --env "SIXEL_CMS_RENDERING_INTENT=perceptual!" \
    --cms-engine=builtin -Llibpng "${input_image}" \
    2>&1 >"${env_output}") || {
    echo "not ok" 1 - "cms_intent environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=cms_intent|stored=1|binding=cms_rendering_intent_order|value=relative+saturation!*}" != "${env_trace}" || {
    echo "not ok" 1 - "cms_intent environment value was not stored"
    exit 0
}

test "${env_trace#*LSXCMS1|intent_order=relative+saturation|exclusive=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "cms_intent environment value did not reach CMS"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "cms_intent short and environment output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "cms_intent image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "cms_intent preserves image output"
exit 0
