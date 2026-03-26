#!/bin/sh
# Verify builtin GIF transparency keeps keycolor unless bgcolor is provided.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose2.gif"
out_default="${ARTIFACT_LOCAL_DIR}/builtin_gif_transparent_default.six"
out_bgcolor="${ARTIFACT_LOCAL_DIR}/builtin_gif_transparent_bgcolor.six"
keycolor_header="$(printf '\033P0;1q')"

run_img2sixel --env SIXEL_THREADS=4 \
              -Lbuiltin! \
              -ldisable -d fs -y raster \
              "${input_gif}" >"${out_default}" || {
    echo "not ok" 1 - "builtin transparent GIF default decode failed"
    exit 0
}

run_img2sixel --env SIXEL_THREADS=4 \
              --env SIXEL_BGCOLOR=white \
              -Lbuiltin! \
              -ldisable -d fs -y raster \
              "${input_gif}" >"${out_bgcolor}" || {
    echo "not ok" 1 - "builtin transparent GIF with bgcolor decode failed"
    exit 0
}

case "$(cat "${out_default}")" in
    *"${keycolor_header}"*)
        default_has_keycolor=1
        ;;
    *)
        default_has_keycolor=0
        ;;
esac

case "$(cat "${out_bgcolor}")" in
    *"${keycolor_header}"*)
        bgcolor_has_keycolor=1
        ;;
    *)
        bgcolor_has_keycolor=0
        ;;
esac

if [ "${default_has_keycolor}" -eq 1 ] &&
   [ "${bgcolor_has_keycolor}" -eq 0 ]; then
    echo "ok" 1 - "builtin GIF transparency keycolor is gated by bgcolor"
else
    echo "not ok" 1 - "builtin GIF transparency keycolor gating mismatch"
fi

exit 0
