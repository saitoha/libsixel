#!/bin/sh
# Verify carry-enabled fixed/varcoeff dithers preserve transparent leading runs.

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
    output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-fs-carry-${file}.six"
    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  --env SIXEL_THREADS=4 \
                  -Llibpng:cms=0! \
                  -d fs -Y carry -y raster \
                  "${input_png}" >"${output_six}" || {
        echo "not ok" "${index}" - "fixed fs carry failed (${file})"
        continue
    }
    prefix="$(extract_prefix <"${output_six}")"
    case "${prefix}" in
        *"!5?"*)
            echo "ok" "${index}" - "fixed fs carry keeps transparent run (${file})"
            ;;
        *)
            echo "not ok" "${index}" - "fixed fs carry lost transparent run (${file})"
            ;;
    esac
done

for file in ${files}; do
    index=$((index + 1))
    input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"
    output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-lso2-carry-${file}.six"
    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  --env SIXEL_THREADS=4 \
                  -Llibpng:cms=0! \
                  -d lso2 -Y carry -y raster \
                  "${input_png}" >"${output_six}" || {
        echo "not ok" "${index}" - "varcoeff lso2 carry failed (${file})"
        continue
    }
    prefix="$(extract_prefix <"${output_six}")"
    case "${prefix}" in
        *"!5?"*)
            echo "ok" "${index}" - "varcoeff lso2 carry keeps transparent run (${file})"
            ;;
        *)
            echo "not ok" "${index}" - "varcoeff lso2 carry lost transparent run (${file})"
            ;;
    esac
done

exit 0
