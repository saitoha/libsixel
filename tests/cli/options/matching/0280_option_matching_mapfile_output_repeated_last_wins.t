#!/bin/sh
# Verify repeated palette-output options use the last occurrence.
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
first_palette="${ARTIFACT_LOCAL_DIR}/first.act"
expected_palette='GIMP Palette
Name: libsixel export
Columns: 16
# Exported by libsixel
  0   0   0	Index 0
255 255 255	Index 1'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bgray1 \
        -M act:"${first_palette}" -M gpl:- \
        -o/dev/null "${input_image}"
) || {
    echo "not ok" 1 - "repeated palette-output option failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "repeated palette-output normalization failed"
    exit 0
}

test ! -e "${first_palette}" || {
    echo "not ok" 1 - "earlier palette-output path was written"
    exit 0
}
test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "last palette-output option did not win"
    exit 0
}

echo "ok" 1 - "repeated palette-output follows last occurrence"
exit 0
