#!/bin/sh
# TAP test: extensionless JASC signature is preferred over ACT size heuristics.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
jasc_file="${TOP_SRCDIR}/tests/data/inputs/mapfile/jasc-invalid-version-size768-noext"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
          -m "${jasc_file}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "extensionless JASC-signature input unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_pal_jasc: invalid version.}" != "${msg}" || {
    echo "not ok" 1 - "missing JASC-signature priority diagnostic"
    exit 0
}

echo "ok" 1 - "extensionless JASC-signature input follows JASC parser"

exit 0
