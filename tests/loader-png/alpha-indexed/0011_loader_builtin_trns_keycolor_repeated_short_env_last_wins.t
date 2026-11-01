#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify repeated short -% assignments honor the last alpha-policy value.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_bmp="${TOP_SRCDIR}/tests/data/inputs/formats/bmp-info40-bi-png-rgba16-2x2.bmp"
out_repeated="${ARTIFACT_LOCAL_DIR}/builtin-alpha-policy-repeated-last-composite.six"
out_composite="${ARTIFACT_LOCAL_DIR}/builtin-alpha-policy-composite.six"
out_keep="${ARTIFACT_LOCAL_DIR}/builtin-alpha-policy-keep.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
              -% SIXEL_ALPHA_POLICY=keep \
              -% SIXEL_ALPHA_POLICY=composite \
              -Lbuiltin:cms_engine=none! -d fs:scan=raster -B#fff \
              "${input_bmp}" >"${out_repeated}" || {
    echo "not ok 1 - builtin repeated short -% alpha-policy render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
              -% SIXEL_ALPHA_POLICY=composite \
              -Lbuiltin:cms_engine=none! -d fs:scan=raster -B#fff \
              "${input_bmp}" >"${out_composite}" || {
    echo "not ok 1 - builtin explicit composite policy render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
              -% SIXEL_ALPHA_POLICY=keep \
              -Lbuiltin:cms_engine=none! -d fs:scan=raster -B#fff \
              "${input_bmp}" >"${out_keep}" || {
    echo "not ok 1 - builtin explicit keep policy render failed"
    exit 0
}

cmp -s "${out_repeated}" "${out_composite}" || {
    echo "not ok 1 - builtin repeated short -% did not use last policy value"
    exit 0
}

cmp -s "${out_repeated}" "${out_keep}" && {
    echo "not ok 1 - builtin repeated short -% did not change policy state"
    exit 0
}

echo "ok 1 - builtin repeated short -% uses last alpha-policy value"

exit 0
