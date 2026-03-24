#!/bin/sh
# Verify tRNS keycolor opt-in gating conditions in libpng loader.

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

echo "1..14"
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

index=0
trns_nonalpha_files="tbbn0g04.png tbwn0g16.png tbrn2c08.png tbbn2c16.png tbgn2c16.png"

for file in ${trns_nonalpha_files}; do
    index=$((index + 1))
    input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"
    output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-gating-cms1-${file}.six"

    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  -Llibpng:cms=1! \
                  -d fs -y raster \
                  "${input_png}" >"${output_six}" || {
        echo "not ok" "${index}" - "cms=1 render failed (${file})"
        continue
    }

    glyph="$(extract_leading_glyph <"${output_six}")"
    if [ "${glyph}" = "~" ]; then
        echo "ok" "${index}" - "cms=1 disables keycolor path (${file})"
    else
        echo "not ok" "${index}" - "cms=1 unexpectedly kept keycolor path (${file}, got=${glyph})"
    fi
done

for file in ${trns_nonalpha_files}; do
    index=$((index + 1))
    input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"
    output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-gating-bgcolor-${file}.six"

    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  -Llibpng:cms=0! \
                  -B '#000000' \
                  -d fs -y raster \
                  "${input_png}" >"${output_six}" || {
        echo "not ok" "${index}" - "bgcolor render failed (${file})"
        continue
    }

    glyph="$(extract_leading_glyph <"${output_six}")"
    if [ "${glyph}" = "~" ]; then
        echo "ok" "${index}" - "bgcolor disables keycolor path (${file})"
    else
        echo "not ok" "${index}" - "bgcolor unexpectedly kept keycolor path (${file}, got=${glyph})"
    fi
done

nontrns_files="basn0g04.png basn0g16.png basn2c08.png basn2c16.png"
for file in ${nontrns_files}; do
    index=$((index + 1))
    input_png="${TOP_SRCDIR}/images/pngsuite/basic/${file}"
    output_off="${ARTIFACT_LOCAL_DIR}/trns-keycolor-gating-no-trns-off-${file}.six"
    output_on="${ARTIFACT_LOCAL_DIR}/trns-keycolor-gating-no-trns-on-${file}.six"

    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
                  -Llibpng:cms=0! \
                  -d fs -y raster \
                  "${input_png}" >"${output_off}" || {
        echo "not ok" "${index}" - "no-tRNS off render failed (${file})"
        continue
    }

    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  -Llibpng:cms=0! \
                  -d fs -y raster \
                  "${input_png}" >"${output_on}" || {
        echo "not ok" "${index}" - "no-tRNS on render failed (${file})"
        continue
    }

    if cmp -s "${output_off}" "${output_on}"; then
        echo "ok" "${index}" - "no-tRNS image is unchanged by opt-in (${file})"
    else
        echo "not ok" "${index}" - "no-tRNS image changed by opt-in (${file})"
    fi
done

exit 0
