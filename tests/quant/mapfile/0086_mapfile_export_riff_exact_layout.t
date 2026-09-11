#!/bin/sh
# Verify RIFF PAL export writes the complete canonical binary layout.
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
actual_palette="${ARTIFACT_LOCAL_DIR}/actual.riff"
expected_palette="${ARTIFACT_LOCAL_DIR}/expected.riff"

printf '\122\111\106\106\030\000\000\000\120\101\114\040\144\141\164\141\014\000\000\000\000\003\002\000\000\000\000\000\377\377\377\000' \
    >"${expected_palette}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bgray1 \
    -M pal-riff:"${actual_palette}" -o /dev/null "${input_image}" || {
    echo "not ok" 1 - "RIFF PAL export failed"
    exit 0
}

cmp -s "${actual_palette}" "${expected_palette}" || {
    echo "not ok" 1 - "RIFF PAL layout changed"
    exit 0
}

echo "ok" 1 - "RIFF PAL export has exact chunks, fields, entries, and flags"
exit 0
