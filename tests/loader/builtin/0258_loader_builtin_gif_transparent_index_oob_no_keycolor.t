#!/bin/sh
# TAP test: out-of-range transparent index is normalized (no keycolor header).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-index-oob-static.gif"
out_six="${ARTIFACT_LOCAL_DIR}/builtin_gif_transparent_index_oob.six"
keycolor_header="$(printf '\033P0;0q')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
              -Lbuiltin! -ldisable -d fs:scan=raster \
              "${input_gif}" >"${out_six}" || {
    echo "not ok" 1 - "transparent-index-oob GIF decode failed"
    exit 0
}

set +x
out_six_text=""
IFS= read -r out_six_text < "${out_six}" || test -n "${out_six_text}"
case "${out_six_text}" in
    *"${keycolor_header}"*)
        echo "not ok" 1 - "transparent-index-oob unexpectedly emitted keycolor header"
        exit 0
        ;;
esac

echo "ok" 1 - "transparent-index-oob is treated as opaque"
exit 0
