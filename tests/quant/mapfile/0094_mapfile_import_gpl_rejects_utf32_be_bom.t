#!/bin/sh
# Verify GPL import rejects a UTF-32 big-endian BOM.
# Test-plan: docs/testing/mapfile-parser-coverage.md
# Policy: docs/functionality/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
input_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/gpl-utf32be-bom-invalid.gpl"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! \
          -m gpl:"${input_palette}" -o/dev/null \
          "${input_image}" 2>&1 >/dev/null) && {
    echo "not ok" 1 - "GPL UTF-32BE BOM unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_gpl: unsupported text encoding.}" \
    != "${msg}" || {
    echo "not ok" 1 - "missing GPL UTF-32BE diagnostic"
    exit 0
}

echo "ok" 1 - "GPL UTF-32BE BOM is rejected"
exit 0
