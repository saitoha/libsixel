#!/bin/sh
# TAP test: default PAL palette export writes JASC-PAL header.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
pal_default="${ARTIFACT_LOCAL_DIR}/palette-default.pal"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -M "${pal_default}" -o "${ARTIFACT_LOCAL_DIR}/pal-default.six"     "${snake_png}" || {
    echo "not ok" 1 - "PAL palette export failed"
    exit 0
}

IFS= read -r pal_header < "${pal_default}" || pal_header=""
case "${pal_header}" in
    *"JASC-PAL"*) ;;
    *)
        echo "not ok" 1 - "PAL palette missing JASC-PAL header"
        exit 0
        ;;
esac

test -n "${pal_header}" || {
    echo "not ok" 1 - "PAL palette missing JASC-PAL header"
    exit 0
}

echo "ok" 1 - "PAL palette exported with JASC-PAL header"

exit 0
