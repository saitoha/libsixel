#!/bin/sh
# Verify repeated --env/-% assignments honor the last value for keycolor opt-in.

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

echo "1..12"
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

check_expected() {
    output_six="$1"
    expect_on="$2"
    case_label="$3"
    file_label="$4"

    hdr="$(extract_header_params <"${output_six}")"
    glyph="$(extract_first_body_glyph <"${output_six}")"

    if [ "${expect_on}" = "1" ]; then
        if [ "${hdr}" = "0;1" ] && [ "${glyph}" = "?" ]; then
            return 0
        fi
    else
        if [ -z "${hdr}" ] && [ "${glyph}" = "~" ]; then
            return 0
        fi
    fi

    echo "not ok" "${index}" - "${case_label} mismatch (${file_label}, hdr=${hdr}, glyph=${glyph})"
    return 1
}

files="tbbn0g04.png tbrn2c08.png"
index=0

for file in ${files}; do
    input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"

    index=$((index + 1))
    out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-repeated-long0-long1-${file}.six"
    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
                  --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  -Llibpng:cms=0! -d fs -y raster \
                  "${input_png}" >"${out}" || {
        echo "not ok" "${index}" - "long0->long1 render failed (${file})"
        continue
    }
    if check_expected "${out}" 1 "long0->long1" "${file}"; then
        echo "ok" "${index}" - "long0->long1 enables keycolor (${file})"
    fi

    index=$((index + 1))
    out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-repeated-long1-long0-${file}.six"
    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
                  -Llibpng:cms=0! -d fs -y raster \
                  "${input_png}" >"${out}" || {
        echo "not ok" "${index}" - "long1->long0 render failed (${file})"
        continue
    }
    if check_expected "${out}" 0 "long1->long0" "${file}"; then
        echo "ok" "${index}" - "long1->long0 disables keycolor (${file})"
    fi

    index=$((index + 1))
    out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-repeated-short0-short1-${file}.six"
    run_img2sixel -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
                  -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  -Llibpng:cms=0! -d fs -y raster \
                  "${input_png}" >"${out}" || {
        echo "not ok" "${index}" - "short0->short1 render failed (${file})"
        continue
    }
    if check_expected "${out}" 1 "short0->short1" "${file}"; then
        echo "ok" "${index}" - "short0->short1 enables keycolor (${file})"
    fi

    index=$((index + 1))
    out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-repeated-short1-short0-${file}.six"
    run_img2sixel -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
                  -Llibpng:cms=0! -d fs -y raster \
                  "${input_png}" >"${out}" || {
        echo "not ok" "${index}" - "short1->short0 render failed (${file})"
        continue
    }
    if check_expected "${out}" 0 "short1->short0" "${file}"; then
        echo "ok" "${index}" - "short1->short0 disables keycolor (${file})"
    fi

    index=$((index + 1))
    out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-repeated-long0-short1-${file}.six"
    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
                  -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  -Llibpng:cms=0! -d fs -y raster \
                  "${input_png}" >"${out}" || {
        echo "not ok" "${index}" - "long0->short1 render failed (${file})"
        continue
    }
    if check_expected "${out}" 1 "long0->short1" "${file}"; then
        echo "ok" "${index}" - "long0->short1 enables keycolor (${file})"
    fi

    index=$((index + 1))
    out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-repeated-short1-long0-${file}.six"
    run_img2sixel -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
                  -Llibpng:cms=0! -d fs -y raster \
                  "${input_png}" >"${out}" || {
        echo "not ok" "${index}" - "short1->long0 render failed (${file})"
        continue
    }
    if check_expected "${out}" 0 "short1->long0" "${file}"; then
        echo "ok" "${index}" - "short1->long0 disables keycolor (${file})"
    fi
done

exit 0
