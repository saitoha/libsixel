#!/bin/sh
# Verify fixed-method matrix preserves transparent leading glyphs (8bit/float32).

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

echo "1..40"
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

files="tbbn0g04.png tbrn2c08.png"
methods="auto none atkinson jajuni stucki burkes sierra1 sierra2 sierra3 a_dither"
index=0

for precision in 8bit float32; do
    for method in ${methods}; do
        for file in ${files}; do
            index=$((index + 1))
            input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"
            output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-fixed-matrix-${precision}-${method}-${file}.six"

            run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                          --env SIXEL_THREADS=4 \
                          -Llibpng:cms=0! \
                          --precision="${precision}" \
                          -d "${method}" -y raster \
                          "${input_png}" >"${output_six}" || {
                echo "not ok" "${index}" - "${method} ${precision} render failed (${file})"
                continue
            }

            glyph="$(extract_leading_glyph <"${output_six}")"
            if [ "${glyph}" = "?" ]; then
                echo "ok" "${index}" - "${method} ${precision} keeps transparent leading glyph (${file})"
            else
                echo "not ok" "${index}" - "${method} ${precision} lost transparent leading glyph (${file}, got=${glyph})"
            fi
        done
    done
done

exit 0
