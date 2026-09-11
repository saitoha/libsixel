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
actual_size=0

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m pal-jasc:"${input_palette}" \
    -M pal-riff:"${actual_palette}" -o/dev/null "${input_image}" || {
    echo "not ok" 1 - "256-color RIFF PAL export failed"
    exit 0
}

actual_size=$(wc -c <"${actual_palette}")
test "${RUNTIME_ENV_IS_OPENVMS-0}" = "1" && {
    actual_size=$(stat -c %s "${actual_palette}")
}
test "${actual_size}" -eq 1048 || {
    echo "not ok" 1 - "256-color RIFF PAL size is not 1048 bytes"
    exit 0
}

# Word splitting is intentional: each hexadecimal byte becomes one argument.
# shellcheck disable=SC2046
set -- $(od -An -tx1 -j20 -N4 "${actual_palette}")
test "$#" -eq 4 && test "$1" = 00 && test "$2" = 03 && \
    test "$3" = 00 && test "$4" = 01 || {
    echo "not ok" 1 - "RIFF PAL 256-color version or count field changed"
    exit 0
}

echo "ok" 1 - "RIFF PAL export writes exact 256-color size and count"
exit 0
