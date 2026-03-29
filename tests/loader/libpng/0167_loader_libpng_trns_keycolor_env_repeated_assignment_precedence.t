#!/bin/sh
# Verify repeated long --env assignments honor the last value.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-repeated-long-last1-tbbn0g04.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -Llibpng:cms_engine=none! -d fs -y raster \
              "${input_png}" >"${out}" || {
    echo "not ok 1 - repeated long --env render failed"
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
        echo "ok 1 - repeated long --env uses last value"
        ;;
    *)
        echo "not ok 1 - repeated long --env did not use last value"
        ;;
esac

exit 0
