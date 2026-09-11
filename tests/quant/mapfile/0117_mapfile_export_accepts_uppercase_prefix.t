#!/bin/sh
# Verify palette-output type prefixes are case-insensitive.
# Test-plan: docs/testing/mapfile-parser-coverage.md
# Policy: docs/functionality/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
expected_palette='GIMP Palette
Name: libsixel export
Columns: 16
# Exported by libsixel
  0   0   0	Index 0
255 255 255	Index 1'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bgray1 \
        -M GPL:- -o/dev/null "${input_image}"
) || {
    echo "not ok" 1 - "uppercase palette-output prefix failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "GPL output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "uppercase prefix did not select GPL output"
    exit 0
}

echo "ok" 1 - "palette-output type prefixes are case-insensitive"
exit 0
