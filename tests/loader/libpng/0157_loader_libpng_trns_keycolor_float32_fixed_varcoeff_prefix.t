#!/bin/sh
# Verify float32 fixed/varcoeff dithers preserve transparent leading glyphs.

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

echo "1..20"
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

for diff in fs lso2; do
    for carry_mode in none carry; do
        for file in ${files}; do
            index=$((index + 1))
            input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"
            output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-f32-${diff}-${carry_mode}-${file}.six"

            if [ "${carry_mode}" = "carry" ]; then
                run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                              --env SIXEL_THREADS=4 \
                              -Llibpng:cms=0! \
                              -d "${diff}" -Y carry -y raster --precision=float32 \
                              "${input_png}" >"${output_six}" || {
                    echo "not ok" "${index}" - "float32 ${diff} carry render failed (${file})"
                    continue
                }
            else
                run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                              --env SIXEL_THREADS=4 \
                              -Llibpng:cms=0! \
                              -d "${diff}" -y raster --precision=float32 \
                              "${input_png}" >"${output_six}" || {
                    echo "not ok" "${index}" - "float32 ${diff} render failed (${file})"
                    continue
                }
            fi

            glyph="$(extract_leading_glyph <"${output_six}")"
            if [ "${glyph}" = "?" ]; then
                echo "ok" "${index}" - "float32 ${diff}/${carry_mode} keeps transparent leading glyph (${file})"
            else
                echo "not ok" "${index}" - "float32 ${diff}/${carry_mode} lost transparent leading glyph (${file}, got=${glyph})"
            fi
        done
    done
done

exit 0
