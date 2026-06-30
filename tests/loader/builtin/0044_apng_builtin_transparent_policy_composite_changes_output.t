#!/bin/sh
# Verify composite transparent policy changes builtin APNG output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_dispose_background.png"
out_default="${ARTIFACT_LOCAL_DIR}/builtin-apng-trns-keycolor-default.six"
out_composite="${ARTIFACT_LOCAL_DIR}/builtin-apng-transparent-policy-composite.six"
out_transparent="${ARTIFACT_LOCAL_DIR}/builtin-apng-transparent-policy-transparent.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
              -Lbuiltin! -d fs:scan=raster \
              -B#fff "${input_png}" >"${out_default}" || {
    echo "not ok 1 - builtin APNG default render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
              --env SIXEL_TRANSPARENT_POLICY=composite \
              -Lbuiltin! -d fs:scan=raster \
              -B#fff "${input_png}" >"${out_composite}" || {
    echo "not ok 1 - builtin APNG composite policy render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
              --env SIXEL_TRANSPARENT_POLICY=transparent \
              -Lbuiltin! -d fs:scan=raster \
              -B#fff "${input_png}" >"${out_transparent}" || {
    echo "not ok 1 - builtin APNG transparent policy render failed"
    exit 0
}

cmp -s "${out_default}" "${out_transparent}" || {
    echo "not ok 1 - builtin APNG default policy is not transparent"
    exit 0
}

cmp -s "${out_default}" "${out_composite}" && {
    echo "not ok 1 - builtin APNG composite policy did not change output"
    exit 0
}

echo "ok 1 - builtin APNG composite policy changes output"

exit 0
