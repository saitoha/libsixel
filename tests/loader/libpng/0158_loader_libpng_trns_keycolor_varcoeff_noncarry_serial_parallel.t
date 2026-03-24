#!/bin/sh
# Verify non-carry varcoeff (lso2) keeps transparent leading glyphs.

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

echo "1..10"
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

files="tbbn0g04.png tbwn0g16.png tbrn2c08.png tbbn2c16.png tbgn2c16.png"
index=0

for file in ${files}; do
    index=$((index + 1))
    input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"
    output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-lso2-serial-${file}.six"

    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  --env SIXEL_THREADS=1 \
                  -Llibpng:cms=0! \
                  -d lso2 -y raster \
                  "${input_png}" >"${output_six}" || {
        echo "not ok" "${index}" - "lso2 serial render failed (${file})"
        continue
    }

    glyph="$(extract_leading_glyph <"${output_six}")"
    if [ "${glyph}" = "?" ]; then
        echo "ok" "${index}" - "lso2 serial keeps transparent leading glyph (${file})"
    else
        echo "not ok" "${index}" - "lso2 serial lost transparent leading glyph (${file}, got=${glyph})"
    fi
done

for file in ${files}; do
    index=$((index + 1))
    input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"
    output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-lso2-parallel-${file}.six"

    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  --env SIXEL_THREADS=4 \
                  -Llibpng:cms=0! \
                  -d lso2 -y raster \
                  "${input_png}" >"${output_six}" || {
        echo "not ok" "${index}" - "lso2 parallel render failed (${file})"
        continue
    }

    glyph="$(extract_leading_glyph <"${output_six}")"
    if [ "${glyph}" = "?" ]; then
        echo "ok" "${index}" - "lso2 parallel keeps transparent leading glyph (${file})"
    else
        echo "not ok" "${index}" - "lso2 parallel lost transparent leading glyph (${file}, got=${glyph})"
    fi
done

exit 0
