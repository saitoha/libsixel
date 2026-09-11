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
actual_size=0

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m pal-jasc:"${input_palette}" \
    -M act:"${actual_palette}" -o/dev/null "${input_image}" || {
    echo "not ok" 1 - "256-color ACT export failed"
    exit 0
}

actual_size=$(wc -c <"${actual_palette}")
test "${RUNTIME_ENV_IS_OPENVMS-0}" = "1" && {
    actual_size=$(stat -c %s "${actual_palette}")
}
test "${actual_size}" -eq 772 || {
    echo "not ok" 1 - "256-color ACT size is not 772 bytes"
    exit 0
}

# Word splitting is intentional: each hexadecimal byte becomes one argument.
# shellcheck disable=SC2046
set -- $(od -An -tx1 -j768 -N4 "${actual_palette}")
test "$#" -eq 4 && test "$1" = 01 && test "$2" = 00 && \
    test "$3" = 00 && test "$4" = 00 || {
    echo "not ok" 1 - "ACT 256-color count or transparency field changed"
    exit 0
}

echo "ok" 1 - "ACT export writes exact 256-color trailer"
exit 0
