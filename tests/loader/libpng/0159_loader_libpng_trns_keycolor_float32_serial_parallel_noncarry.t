#!/bin/sh
# Verify float32 non-carry fixed/varcoeff keep transparent leading glyphs in serial/parallel.

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
    for threads in 1 4; do
        for file in ${files}; do
            index=$((index + 1))
            input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"
            output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-f32-noncarry-${diff}-t${threads}-${file}.six"

            run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                          --env SIXEL_THREADS="${threads}" \
                          -Llibpng:cms=0! \
                          -d "${diff}" -y raster --precision=float32 \
                          "${input_png}" >"${output_six}" || {
                echo "not ok" "${index}" - "float32 ${diff} threads=${threads} render failed (${file})"
                continue
            }

            glyph="$(extract_leading_glyph <"${output_six}")"
            if [ "${glyph}" = "?" ]; then
                echo "ok" "${index}" - "float32 ${diff} threads=${threads} keeps transparent leading glyph (${file})"
            else
                echo "not ok" "${index}" - "float32 ${diff} threads=${threads} lost transparent leading glyph (${file}, got=${glyph})"
            fi
        done
    done
done

exit 0
