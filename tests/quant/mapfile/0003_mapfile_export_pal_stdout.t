#!/bin/sh
# TAP test: PAL export to stdout retains JASC-PAL header.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
pal_stdout="${ARTIFACT_LOCAL_DIR}/palette-stdout.pal"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -M pal:- -o "${ARTIFACT_LOCAL_DIR}/pal-stdout.six"     "${snake_png}" >"${pal_stdout}" || {
    echo "not ok" 1 - "PAL stdout export failed"
    exit 0
}

IFS= read -r pal_header < "${pal_stdout}" || pal_header=""
case "${pal_header}" in
    *"JASC-PAL"*) ;;
    *)
        echo "not ok" 1 - "PAL stdout export missing JASC-PAL header"
        exit 0
        ;;
esac

test -n "${pal_header}" || {
    echo "not ok" 1 - "PAL stdout export missing JASC-PAL header"
    exit 0
}

echo "ok" 1 - "PAL stdout export emitted JASC-PAL header"

exit 0
