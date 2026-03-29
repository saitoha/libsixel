#!/bin/sh
# Verify repeated short -% assignments honor the last value.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
out="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-repeated-short-last0-tbbn0g04.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Lbuiltin:cms_engine=none! -d fs -y raster \
              "${input_png}" >"${out}" || {
    echo "not ok 1 - builtin repeated short -% render failed"
    exit 0
}

out_text=""
while IFS= read -r out_line || test -n "
${out_line}"; do
    case "${out_text}" in
        "")
            out_text=${out_line}
            ;;
        *)
            out_text="${out_text}
${out_line}"
            ;;
    esac
done < "${out}"
case "${out_text}" in
    *"$(printf '\033')P0;1q"*)
        echo "not ok 1 - builtin repeated short -% did not use last value"
        ;;
    *)
        echo "ok 1 - builtin repeated short -% uses last value"
        ;;
esac

exit 0
