#!/bin/sh
# Verify GPL import retains exactly 256 ordered entries.
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
input_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/gpl-256-valid.gpl"
expected_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/pal-256-valid.pal"
actual_palette="${ARTIFACT_LOCAL_DIR}/actual.pal"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m gpl:"${input_palette}" \
    -M pal-jasc:"${actual_palette}" -o/dev/null "${input_image}" || {
    echo "not ok" 1 - "256-color GPL import failed"
    exit 0
}

cmp -s "${actual_palette}" "${expected_palette}" || {
    echo "not ok" 1 - "GPL did not retain 256 ordered entries"
    exit 0
}

echo "ok" 1 - "GPL retains exactly 256 ordered entries"
exit 0
