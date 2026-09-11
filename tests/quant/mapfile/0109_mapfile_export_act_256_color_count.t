#!/bin/sh
# Verify ACT export writes the 256-color big-endian count.
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
input_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/pal-256-valid.pal"
actual_palette="${ARTIFACT_LOCAL_DIR}/actual.act"
actual_trailer="${ARTIFACT_LOCAL_DIR}/actual-trailer.bin"
expected_trailer="${ARTIFACT_LOCAL_DIR}/expected-trailer.bin"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m pal-jasc:"${input_palette}" \
    -M act:"${actual_palette}" -o/dev/null "${input_image}" || {
    echo "not ok" 1 - "256-color ACT export failed"
    exit 0
}

test "$(wc -c <"${actual_palette}")" -eq 772 || {
    echo "not ok" 1 - "256-color ACT size is not 772 bytes"
    exit 0
}

dd if="${actual_palette}" of="${actual_trailer}" bs=1 skip=768 count=4 \
    2>/dev/null || {
    echo "not ok" 1 - "ACT trailer extraction failed"
    exit 0
}
printf '\001\000\000\000' >"${expected_trailer}"
cmp -s "${actual_trailer}" "${expected_trailer}" || {
    echo "not ok" 1 - "ACT 256-color count or transparency field changed"
    exit 0
}

echo "ok" 1 - "ACT export writes exact 256-color trailer"
exit 0
