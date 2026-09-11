#!/bin/sh
# Verify RIFF PAL import skips a padded odd-sized unknown subchunk.
# Test-plan: docs/testing/mapfile-parser-coverage.md
# Policy: docs/functionality/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
input_palette="${ARTIFACT_LOCAL_DIR}/unknown-chunk.pal"
expected_palette='JASC-PAL
0100
2
12 34 56
200 210 220'

printf '\122\111\106\106\044\000\000\000\120\101\114\040\112\125\116\113\003\000\000\000\141\142\143\000\144\141\164\141\014\000\000\000\000\003\002\000\014\042\070\177\310\322\334\200' \
    >"${input_palette}"

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m pal-riff:"${input_palette}" \
        -M pal-jasc:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "RIFF PAL padded unknown chunk import failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "RIFF PAL output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "RIFF PAL unknown chunk changed imported entries"
    exit 0
}

echo "ok" 1 - "RIFF PAL skips a padded odd-sized unknown chunk"
exit 0
