#!/bin/sh
# Verify an independent RIFF PAL imports ordered RGB and ignores entry flags.
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
input_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/riff-two-colors-flags-nonzero.pal"
expected_palette='JASC-PAL
0100
2
12 34 56
200 210 220'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m pal-riff:"${input_palette}" \
        -M pal-jasc:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "independent RIFF PAL import failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "RIFF PAL output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "RIFF PAL entries or flags were interpreted incorrectly"
    exit 0
}

echo "ok" 1 - "RIFF PAL imports ordered RGB and ignores entry flags"
exit 0
