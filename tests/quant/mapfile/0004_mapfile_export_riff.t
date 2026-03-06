#!/bin/sh
# TAP test: RIFF palette export writes RIFF header bytes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
riff_palette="${ARTIFACT_LOCAL_DIR}/palette-riff.pal"

run_img2sixel -M pal-riff:"${riff_palette}" \
    -o "${ARTIFACT_LOCAL_DIR}/pal-riff.six" "${snake_png}" || {
    echo "not ok" 1 - "RIFF palette export failed"
    exit 0
}

dd if="${riff_palette}" bs=1 count=4 2>/dev/null | awk '
    BEGIN {
        ORS = ""
    }
    {
        header = header $0
    }
    END {
        if (header != "RIFF") {
            exit 1
        }
    }
' || {
    echo "not ok" 1 - "RIFF palette export missing RIFF header"
    exit 0
}

echo "ok" 1 - "RIFF palette export has RIFF header"

exit 0
