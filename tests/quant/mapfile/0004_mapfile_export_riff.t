#!/bin/sh
# TAP test: RIFF palette export honours type prefix.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
riff_palette="${ARTIFACT_LOCAL_DIR}/palette-riff.pal"

if run_img2sixel -M pal-riff:"${riff_palette}" \
        -o "${ARTIFACT_LOCAL_DIR}/pal-riff.six" "${snake_png}"; then
    riff_header=$(dd if="${riff_palette}" bs=1 count=4 2>/dev/null |
        LC_ALL=C od -An -tx1 | tr -d ' \n')
    if [ "${riff_header}" = "52494646" ]; then
        pass 1 "RIFF palette export honours type prefix"
    else
        fail 1 "RIFF palette header incorrect (${riff_header})"
    fi
else
    fail 1 "RIFF palette export failed"
fi

exit 0
