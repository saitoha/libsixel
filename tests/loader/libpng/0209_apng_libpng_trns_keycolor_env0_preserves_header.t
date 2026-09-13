#!/bin/sh
# Policy: docs/loader/libpng.md
# Verify APNG tRNS coverage requests P2=1 under keep, including keycolor off.
set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng is required"
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is required"
    exit 0
}
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_png="${ARTIFACT_LOCAL_DIR}/apng-trns-keycolor-env0.png"
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/libpng_contract emit_trns \
    "${input_png}" || {
    echo "not ok 1 - APNG fixture generation failed"
    exit 0
}
output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Akeep \
    -Llibpng:cms_engine=none! \
    --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 "${input_png}") || {
    echo "not ok 1 - APNG conversion failed"
    exit 0
}
header=$(printf '\033P0;1q')
test "${output#*"${header}"}" != "${output}" || {
    echo "not ok 1 - APNG lost the keep header"
    exit 0
}
echo "ok 1 - APNG tRNS retains the keep header"
exit 0
