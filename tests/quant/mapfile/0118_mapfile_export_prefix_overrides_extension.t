#!/bin/sh
# Verify a palette-output type prefix overrides a mismatched extension.
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
actual_palette="${ARTIFACT_LOCAL_DIR}/actual.act"
expected_palette="${ARTIFACT_LOCAL_DIR}/expected.gpl"

printf '%s\n' 'GIMP Palette
Name: libsixel export
Columns: 16
# Exported by libsixel
  0   0   0	Index 0
255 255 255	Index 1' >"${expected_palette}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bgray1 \
    -M gpl:"${actual_palette}" -o/dev/null "${input_image}" || {
    echo "not ok" 1 - "palette-output prefix override failed"
    exit 0
}

cmp -s "${actual_palette}" "${expected_palette}" || {
    echo "not ok" 1 - "palette-output extension overrode explicit prefix"
    exit 0
}

echo "ok" 1 - "palette-output prefix overrides a mismatched extension"
exit 0
