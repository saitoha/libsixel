#!/bin/sh
# TAP test: GPL import accepts UTF-8 BOM.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/gpl-utf8-bom-valid.gpl"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin -m "${gpl_palette}" \
    "${snake_png}" -o/dev/null || {
    echo "not ok" 1 - "GPL UTF-8 BOM should be accepted"
    exit 0
}

echo "ok" 1 - "GPL UTF-8 BOM is accepted"

exit 0
