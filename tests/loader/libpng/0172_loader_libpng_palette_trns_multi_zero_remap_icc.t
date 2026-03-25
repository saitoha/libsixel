#!/bin/sh
# Verify indexed+tRNS merges multiple zero-alpha indexes and keeps behavior with cms.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..8"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

extract_leading_glyph() {
    perl -0777 -ne '
        $s = $_;
        $s =~ s/^.*?(?=#\d+;2;)//s;
        $s =~ s/^(?:#\d+;2;\d+;\d+;\d+)+//;
        if ($s =~ /^#\d+(?:!\d+)?(.)/s) {
            print $1;
        }
    '
}

input_multi="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-multi0-semi-icc.png"
input_single="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-single0-semi-icc.png"
index=0

for cms in 0 1; do
    index=$((index + 1))
    out_multi_white="${ARTIFACT_LOCAL_DIR}/multi0-white-cms${cms}.six"
    out_single_white="${ARTIFACT_LOCAL_DIR}/single0-white-cms${cms}.six"
    run_img2sixel -Llibpng:cms=${cms}! \
                  -B#ffffff \
                  -d none -p256 \
                  "${input_multi}" >"${out_multi_white}" || {
        echo "not ok" "${index}" - "multi input render failed (cms=${cms})"
        continue
    }
    run_img2sixel -Llibpng:cms=${cms}! \
                  -B#ffffff \
                  -d none -p256 \
                  "${input_single}" >"${out_single_white}" || {
        echo "not ok" "${index}" - "single input render failed (cms=${cms})"
        continue
    }
    if cmp -s "${out_multi_white}" "${out_single_white}"; then
        echo "ok" "${index}" - "multi-zero remap matches normalized reference (cms=${cms})"
    else
        echo "not ok" "${index}" - "multi-zero remap mismatch (cms=${cms})"
    fi

    index=$((index + 1))
    glyph="$(extract_leading_glyph <"${out_multi_white}")"
    if [ "${glyph}" = "?" ]; then
        echo "ok" "${index}" - "leading transparent glyph kept on ICC fixture (cms=${cms})"
    else
        echo "not ok" "${index}" - "leading glyph lost transparency on ICC fixture (cms=${cms}, got=${glyph})"
    fi

    index=$((index + 1))
    out_multi_black="${ARTIFACT_LOCAL_DIR}/multi0-black-cms${cms}.six"
    run_img2sixel -Llibpng:cms=${cms}! \
                  -B#000000 \
                  -d none -p256 \
                  "${input_multi}" >"${out_multi_black}" || {
        echo "not ok" "${index}" - "black background render failed (cms=${cms})"
        continue
    }
    if cmp -s "${out_multi_black}" "${out_multi_white}"; then
        echo "not ok" "${index}" - "semi-transparent composition ignored background (cms=${cms})"
    else
        echo "ok" "${index}" - "semi-transparent composition follows background (cms=${cms})"
    fi

    index=$((index + 1))
    glyph_black="$(extract_leading_glyph <"${out_multi_black}")"
    if [ "${glyph_black}" = "?" ]; then
        echo "ok" "${index}" - "background change still keeps transparent keycolor (cms=${cms})"
    else
        echo "not ok" "${index}" - "background change broke transparent keycolor (cms=${cms}, got=${glyph_black})"
    fi
done

exit 0
