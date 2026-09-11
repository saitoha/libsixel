#!/bin/sh
# Verify GPL import rejects a UTF-16 big-endian BOM.
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

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_file="${TOP_SRCDIR}/tests/data/inputs/mapfile/gpl-utf16-bom-invalid.gpl"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m gpl:"${gpl_file}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "GPL UTF-16BE BOM unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_gpl: unsupported text encoding.}" != "${msg}" || {
    echo "not ok" 1 - "missing GPL UTF-16BE diagnostic"
    exit 0
}

echo "ok" 1 - "GPL UTF-16BE BOM is rejected"

exit 0
