#!/bin/sh
# Verify x_dither positional modes keep transparent leading runs (8bit/float32).

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

extract_prefix() {
    perl -0777 -ne '
        $s = $_;
        $s =~ s/^.*?(?=#\d+;2;)//s;
        $s =~ s/^(?:#\d+;2;\d+;\d+;\d+)+//;
        print substr($s, 0, 80);
    '
}

files="tbbn0g04.png tbwn0g16.png tbrn2c08.png tbbn2c16.png tbgn2c16.png"
index=0

for file in ${files}; do
    index=$((index + 1))
    input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"
    output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-xdither-8bit-${file}.six"
    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  --env SIXEL_THREADS=4 \
                  -Llibpng:cms=0! \
                  -d x_dither -y raster --precision=8bit \
                  "${input_png}" >"${output_six}" || {
        echo "not ok" "${index}" - "x_dither 8bit failed (${file})"
        continue
    }
    prefix="$(extract_prefix <"${output_six}")"
    case "${prefix}" in
        *"!5?"*)
            echo "ok" "${index}" - "x_dither 8bit keeps transparent run (${file})"
            ;;
        *)
            echo "not ok" "${index}" - "x_dither 8bit lost transparent run (${file})"
            ;;
    esac
done

for file in ${files}; do
    index=$((index + 1))
    input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"
    output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-xdither-float32-${file}.six"
    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  --env SIXEL_THREADS=4 \
                  -Llibpng:cms=0! \
                  -d x_dither -y raster --precision=float32 \
                  "${input_png}" >"${output_six}" || {
        echo "not ok" "${index}" - "x_dither float32 failed (${file})"
        continue
    }
    prefix="$(extract_prefix <"${output_six}")"
    case "${prefix}" in
        *"!5?"*)
            echo "ok" "${index}" - "x_dither float32 keeps transparent run (${file})"
            ;;
        *)
            echo "not ok" "${index}" - "x_dither float32 lost transparent run (${file})"
            ;;
    esac
done

exit 0
