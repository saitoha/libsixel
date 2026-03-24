#!/bin/sh
# Verify per-invocation env options override process env for tRNS keycolor opt-in.

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
index=0

for file in ${files}; do
    input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"

    index=$((index + 1))
    out0="${ARTIFACT_LOCAL_DIR}/trns-keycolor-cli-env-proc0-${file}.six"
    SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
        run_img2sixel -Llibpng:cms=0! -d fs -y raster \
                      "${input_png}" >"${out0}" || {
        echo "not ok" "${index}" - "process env=0 render failed (${file})"
        continue
    }
    g="$(extract_leading_glyph <"${out0}")"
    if [ "${g}" = "~" ]; then
        echo "ok" "${index}" - "process env=0 keeps opaque leading glyph (${file})"
    else
        echo "not ok" "${index}" - "process env=0 unexpected leading glyph (${file}, got=${g})"
    fi

    index=$((index + 1))
    out1="${ARTIFACT_LOCAL_DIR}/trns-keycolor-cli-env-proc0-long1-${file}.six"
    SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
        run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                      -Llibpng:cms=0! -d fs -y raster \
                      "${input_png}" >"${out1}" || {
        echo "not ok" "${index}" - "process env=0 + --env=1 render failed (${file})"
        continue
    }
    g="$(extract_leading_glyph <"${out1}")"
    if [ "${g}" = "?" ]; then
        echo "ok" "${index}" - "--env=1 overrides process env=0 (${file})"
    else
        echo "not ok" "${index}" - "--env=1 did not override process env=0 (${file}, got=${g})"
    fi

    index=$((index + 1))
    out2="${ARTIFACT_LOCAL_DIR}/trns-keycolor-cli-env-proc0-short1-${file}.six"
    SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
        run_img2sixel -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                      -Llibpng:cms=0! -d fs -y raster \
                      "${input_png}" >"${out2}" || {
        echo "not ok" "${index}" - "process env=0 + -%=1 render failed (${file})"
        continue
    }
    g="$(extract_leading_glyph <"${out2}")"
    if [ "${g}" = "?" ]; then
        echo "ok" "${index}" - "-%=1 overrides process env=0 (${file})"
    else
        echo "not ok" "${index}" - "-%=1 did not override process env=0 (${file}, got=${g})"
    fi

    index=$((index + 1))
    out3="${ARTIFACT_LOCAL_DIR}/trns-keycolor-cli-env-proc1-${file}.six"
    SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
        run_img2sixel -Llibpng:cms=0! -d fs -y raster \
                      "${input_png}" >"${out3}" || {
        echo "not ok" "${index}" - "process env=1 render failed (${file})"
        continue
    }
    g="$(extract_leading_glyph <"${out3}")"
    if [ "${g}" = "?" ]; then
        echo "ok" "${index}" - "process env=1 enables transparent leading glyph (${file})"
    else
        echo "not ok" "${index}" - "process env=1 unexpected leading glyph (${file}, got=${g})"
    fi

    index=$((index + 1))
    out4="${ARTIFACT_LOCAL_DIR}/trns-keycolor-cli-env-proc1-long0-${file}.six"
    SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
        run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
                      -Llibpng:cms=0! -d fs -y raster \
                      "${input_png}" >"${out4}" || {
        echo "not ok" "${index}" - "process env=1 + --env=0 render failed (${file})"
        continue
    }
    g="$(extract_leading_glyph <"${out4}")"
    if [ "${g}" = "~" ]; then
        echo "ok" "${index}" - "--env=0 overrides process env=1 (${file})"
    else
        echo "not ok" "${index}" - "--env=0 did not override process env=1 (${file}, got=${g})"
    fi

    index=$((index + 1))
    out5="${ARTIFACT_LOCAL_DIR}/trns-keycolor-cli-env-proc1-short0-${file}.six"
    SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
        run_img2sixel -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
                      -Llibpng:cms=0! -d fs -y raster \
                      "${input_png}" >"${out5}" || {
        echo "not ok" "${index}" - "process env=1 + -%=0 render failed (${file})"
        continue
    }
    g="$(extract_leading_glyph <"${out5}")"
    if [ "${g}" = "~" ]; then
        echo "ok" "${index}" - "-%=0 overrides process env=1 (${file})"
    else
        echo "not ok" "${index}" - "-%=0 did not override process env=1 (${file}, got=${g})"
    fi
done

exit 0
