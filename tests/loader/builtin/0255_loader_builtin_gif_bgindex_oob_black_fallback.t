#!/bin/sh
# TAP test: invalid GIF bgindex falls back to deterministic black background.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

valid_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-bgindex-valid-black-anim.gif"
oob_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-bgindex-oob-anim.gif"
out_valid="${ARTIFACT_LOCAL_DIR}/builtin_gif_bgindex_valid.six"
out_oob="${ARTIFACT_LOCAL_DIR}/builtin_gif_bgindex_oob.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
              -Lbuiltin! -S -ldisable -d none -p 256 -y raster \
              "${valid_gif}" >"${out_valid}" || {
    echo "not ok" 1 - "builtin valid bgindex decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
              -Lbuiltin! -S -ldisable -d none -p 256 -y raster \
              "${oob_gif}" >"${out_oob}" || {
    echo "not ok" 1 - "builtin oob bgindex decode failed"
    exit 0
}

cmp -s "${out_valid}" "${out_oob}" || {
    echo "not ok" 1 - "oob bgindex output differs from black fallback reference"
    exit 0
}

echo "ok" 1 - "builtin oob bgindex uses deterministic black fallback"
exit 0
