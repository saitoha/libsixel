#!/bin/sh
# Verify PNG keycolor policy through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|NULL|trns_keycolor
# Registry binding: png_trns_keycolor

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/images/pngsuite/transparency/tbrn2c08.png"
reference_image="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0085_pngsuite_trns_keycolor_tbrn2c08_msssim.ppm"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0099-loader-png-trns-keycolor-short-$$.six"
env_output="${artifact_dir}/0099-loader-png-trns-keycolor-env-$$.six"
control_output="${artifact_dir}/0099-loader-png-trns-keycolor-control-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibpng:K1:cms_engine=none!" "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "trns_keycolor short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=trns_keycolor|stored=1|binding=png_trns_keycolor|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "trns_keycolor short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1" \
    "-Llibpng:cms_engine=none!" "${input_image}" \
    2>&1 >"${env_output}") || {
    echo "not ok" 1 - "trns_keycolor environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=trns_keycolor|stored=1|binding=png_trns_keycolor|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "trns_keycolor environment value was not stored"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Llibpng:K0:cms_engine=none!" "${input_image}" \
    >"${control_output}" || {
    echo "not ok" 1 - "trns_keycolor control conversion failed"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "trns_keycolor short and env output differ"
    exit 0
}

cmp -s "${short_output}" "${control_output}" && {
    echo "not ok" 1 - "trns_keycolor did not affect image output"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "trns_keycolor image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "trns_keycolor preserves image output"
exit 0
