#!/bin/sh
# Verify indexed+tRNS keeps transparent keycolor even with background and cms.

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

files="tbbn3p08.png tbgn3p08.png tbwn3p08.png tbyn3p08.png tm3n3p02.png"
index=0

for cms in 0 1; do
    for file in ${files}; do
        index=$((index + 1))
        input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"
        output_six="${ARTIFACT_LOCAL_DIR}/palette-trns-bg-cms${cms}-${file}.six"

        run_img2sixel -Llibpng:cms=${cms}! \
                      -B#ffffff \
                      -d fs -y raster \
                      "${input_png}" >"${output_six}" || {
            echo "not ok" "${index}" - "render failed (cms=${cms}, file=${file})"
            continue
        }

        glyph="$(extract_leading_glyph <"${output_six}")"
        if [ "${glyph}" = "?" ]; then
            echo "ok" "${index}" - "leading transparent glyph kept (cms=${cms}, file=${file})"
        else
            echo "not ok" "${index}" - "leading glyph lost transparency (cms=${cms}, file=${file}, got=${glyph})"
        fi
    done
done

exit 0
