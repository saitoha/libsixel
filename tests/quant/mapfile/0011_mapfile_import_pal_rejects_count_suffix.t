#!/bin/sh
# TAP test: JASC-PAL import rejects color counts with trailing garbage.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
pal_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/pal-invalid-count-suffix.pal"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
          -m "${pal_palette}" "${snake_png}" \
          -o/dev/null 2>&1) && {
    echo "not ok" 1 - "PAL color count suffix unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_palette_parse_pal_jasc: invalid color count.}" != "${msg}" || {
    echo "not ok" 1 - "missing PAL color count suffix diagnostic"
    exit 0
}

echo "ok" 1 - "PAL color count suffix is rejected"

exit 0
