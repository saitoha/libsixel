#!/bin/sh
# Verify builtin GIF transparency keeps keycolor unless bgcolor is provided.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose2.gif"
out_default="${ARTIFACT_LOCAL_DIR}/builtin_gif_transparent_default.six"
out_bgcolor="${ARTIFACT_LOCAL_DIR}/builtin_gif_transparent_bgcolor.six"
keycolor_header="$(printf '\033P0;1q')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
              -Lbuiltin! \
              -ldisable -d fs -y raster \
              "${input_gif}" >"${out_default}" || {
    echo "not ok" 1 - "builtin transparent GIF default decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
              --env SIXEL_BGCOLOR=white \
              -Lbuiltin! \
              -ldisable -d fs -y raster \
              "${input_gif}" >"${out_bgcolor}" || {
    echo "not ok" 1 - "builtin transparent GIF with bgcolor decode failed"
    exit 0
}

out_default_text=""
while IFS= read -r out_default_line || test -n "
${out_default_line}"; do
    case "${out_default_text}" in
        "")
            out_default_text=${out_default_line}
            ;;
        *)
            out_default_text="${out_default_text}
${out_default_line}"
            ;;
    esac
done < "${out_default}"
case "${out_default_text}" in
    *"${keycolor_header}"*)
        default_has_keycolor=1
        ;;
    *)
        default_has_keycolor=0
        ;;
esac

out_bgcolor_text=""
while IFS= read -r out_bgcolor_line || test -n "
${out_bgcolor_line}"; do
    case "${out_bgcolor_text}" in
        "")
            out_bgcolor_text=${out_bgcolor_line}
            ;;
        *)
            out_bgcolor_text="${out_bgcolor_text}
${out_bgcolor_line}"
            ;;
    esac
done < "${out_bgcolor}"
case "${out_bgcolor_text}" in
    *"${keycolor_header}"*)
        bgcolor_has_keycolor=1
        ;;
    *)
        bgcolor_has_keycolor=0
        ;;
esac

test "${default_has_keycolor}" -eq 1 || {
    echo "not ok" 1 - "builtin GIF transparency keycolor gating mismatch"
    exit 0
}

test "${bgcolor_has_keycolor}" -eq 0 || {
    echo "not ok" 1 - "builtin GIF transparency keycolor gating mismatch"
    exit 0
}

echo "ok" 1 - "builtin GIF transparency keycolor is gated by bgcolor"

exit 0
