#!/bin/sh
# Verify quality mode does not break tRNS keycolor behavior.

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

echo "1..32"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

extract_header_params() {
    perl -0777 -ne '
        if (/^\eP([^q]*)q/s) {
            print $1;
        }
    '
}

extract_first_body_glyph() {
    perl -0777 -ne '
        $s = $_;
        $s =~ s/^.*?\eP[^q]*q//s;
        $s =~ s/^"[0-9;]*//s;
        $s =~ s/^(?:#\d+;2;\d+;\d+;\d+)+//;
        if ($s =~ /^(?:#\d+(?:!\d+)?)?(?:!\d+)?([?-~])/s) {
            print $1;
        }
    '
}

files="tbbn0g04.png tbrn2c08.png"
quality_modes="auto low high full"
methods="fs lso2"
index=0

for file in ${files}; do
    input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"

    for method in ${methods}; do
        for qmode in ${quality_modes}; do
            index=$((index + 1))
            out_on="${ARTIFACT_LOCAL_DIR}/trns-keycolor-quality-on-${method}-${qmode}-${file}.six"
            run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                          --env SIXEL_THREADS=4 \
                          -Llibpng:cms=0! \
                          -d "${method}" -q "${qmode}" -y raster \
                          "${input_png}" >"${out_on}" || {
                echo "not ok" "${index}" - "opt-in render failed (${method}, q=${qmode}, ${file})"
                continue
            }
            hdr="$(extract_header_params <"${out_on}")"
            glyph="$(extract_first_body_glyph <"${out_on}")"
            if [ "${hdr}" = "0;1" ] && [ "${glyph}" = "?" ]; then
                echo "ok" "${index}" - "opt-in keeps keycolor (${method}, q=${qmode}, ${file})"
            else
                echo "not ok" "${index}" - "opt-in lost keycolor (${method}, q=${qmode}, ${file}, hdr=${hdr}, glyph=${glyph})"
            fi

            index=$((index + 1))
            out_off="${ARTIFACT_LOCAL_DIR}/trns-keycolor-quality-off-${method}-${qmode}-${file}.six"
            run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
                          --env SIXEL_THREADS=4 \
                          -Llibpng:cms=0! \
                          -d "${method}" -q "${qmode}" -y raster \
                          "${input_png}" >"${out_off}" || {
                echo "not ok" "${index}" - "opt-out render failed (${method}, q=${qmode}, ${file})"
                continue
            }
            hdr="$(extract_header_params <"${out_off}")"
            glyph="$(extract_first_body_glyph <"${out_off}")"
            if [ -z "${hdr}" ] && [ "${glyph}" = "~" ]; then
                echo "ok" "${index}" - "opt-out remains opaque (${method}, q=${qmode}, ${file})"
            else
                echo "not ok" "${index}" - "opt-out unexpectedly changed (${method}, q=${qmode}, ${file}, hdr=${hdr}, glyph=${glyph})"
            fi
        done
    done
done

exit 0
