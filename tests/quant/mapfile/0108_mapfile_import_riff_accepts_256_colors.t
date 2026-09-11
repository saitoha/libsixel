#!/bin/sh
# Verify RIFF PAL import retains exactly 256 ordered entries.
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
expected_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/pal-256-valid.pal"
input_palette="${ARTIFACT_LOCAL_DIR}/input.riff"
actual_palette="${ARTIFACT_LOCAL_DIR}/actual.pal"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m pal-jasc:"${expected_palette}" \
    -M pal-riff:"${input_palette}" -o/dev/null "${input_image}" || {
    echo "not ok" 1 - "256-color RIFF PAL fixture preparation failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m pal-riff:"${input_palette}" \
    -M pal-jasc:"${actual_palette}" -o/dev/null "${input_image}" || {
    echo "not ok" 1 - "256-color RIFF PAL import failed"
    exit 0
}

cmp -s "${actual_palette}" "${expected_palette}" || {
    echo "not ok" 1 - "RIFF PAL did not retain 256 ordered entries"
    exit 0
}

echo "ok" 1 - "RIFF PAL retains exactly 256 ordered entries"
exit 0
