#!/bin/sh
# Verify tRNS keycolor opt-in accepts only 1 and otherwise stays disabled.

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

echo "1..25"
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
    input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"

    index=$((index + 1))
    output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-env-1-${file}.six"
    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  --env SIXEL_THREADS=4 \
                  -Llibpng:cms=0! \
                  -d fs -y raster \
                  "${input_png}" >"${output_six}" || {
        echo "not ok" "${index}" - "env=1 render failed (${file})"
        continue
    }
    glyph="$(extract_leading_glyph <"${output_six}")"
    if [ "${glyph}" = "?" ]; then
        echo "ok" "${index}" - "env=1 enables transparent leading glyph (${file})"
    else
        echo "not ok" "${index}" - "env=1 did not enable transparent leading glyph (${file}, got=${glyph})"
    fi

    for mode in unset 0 true 2; do
        index=$((index + 1))
        output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-env-${mode}-${file}.six"

        if [ "${mode}" = "unset" ]; then
            run_img2sixel --env SIXEL_THREADS=4 \
                          -Llibpng:cms=0! \
                          -d fs -y raster \
                          "${input_png}" >"${output_six}" || {
                echo "not ok" "${index}" - "env unset render failed (${file})"
                continue
            }
        else
            run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR="${mode}" \
                          --env SIXEL_THREADS=4 \
                          -Llibpng:cms=0! \
                          -d fs -y raster \
                          "${input_png}" >"${output_six}" || {
                echo "not ok" "${index}" - "env=${mode} render failed (${file})"
                continue
            }
        fi

        glyph="$(extract_leading_glyph <"${output_six}")"
        if [ "${glyph}" = "~" ]; then
            echo "ok" "${index}" - "env=${mode} keeps opaque leading glyph (${file})"
        else
            echo "not ok" "${index}" - "env=${mode} unexpectedly changed leading glyph (${file}, got=${glyph})"
        fi
    done
done

exit 0
