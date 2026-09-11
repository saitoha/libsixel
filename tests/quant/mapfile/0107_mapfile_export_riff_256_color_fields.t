#!/bin/sh
# Verify RIFF PAL export writes the 256-color size and count fields.
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
actual_palette="${ARTIFACT_LOCAL_DIR}/actual.riff"
actual_fields="${ARTIFACT_LOCAL_DIR}/actual-fields.bin"
expected_fields="${ARTIFACT_LOCAL_DIR}/expected-fields.bin"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m pal-jasc:"${input_palette}" \
    -M pal-riff:"${actual_palette}" -o/dev/null "${input_image}" || {
    echo "not ok" 1 - "256-color RIFF PAL export failed"
    exit 0
}

test "$(wc -c <"${actual_palette}")" -eq 1048 || {
    echo "not ok" 1 - "256-color RIFF PAL size is not 1048 bytes"
    exit 0
}

dd if="${actual_palette}" of="${actual_fields}" bs=1 skip=20 count=4 \
    2>/dev/null || {
    echo "not ok" 1 - "RIFF PAL version and count extraction failed"
    exit 0
}
printf '\000\003\000\001' >"${expected_fields}"
cmp -s "${actual_fields}" "${expected_fields}" || {
    echo "not ok" 1 - "RIFF PAL 256-color version or count field changed"
    exit 0
}

echo "ok" 1 - "RIFF PAL export writes exact 256-color size and count"
exit 0
