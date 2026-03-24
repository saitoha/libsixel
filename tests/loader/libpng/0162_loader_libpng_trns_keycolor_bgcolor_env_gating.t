#!/bin/sh
# Verify background color env settings disable tRNS keycolor path.

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

echo "1..6"
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
    out_no_bg="${ARTIFACT_LOCAL_DIR}/trns-keycolor-bg-env-none-${file}.six"
    (
        unset SIXEL_BGCOLOR
        run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                      -Llibpng:cms=0! -d fs -y raster \
                      "${input_png}" >"${out_no_bg}"
    ) || {
        echo "not ok" "${index}" - "no-bg render failed (${file})"
        continue
    }
    g="$(extract_leading_glyph <"${out_no_bg}")"
    if [ "${g}" = "?" ]; then
        echo "ok" "${index}" - "no-bg keeps keycolor path enabled (${file})"
    else
        echo "not ok" "${index}" - "no-bg unexpectedly disabled keycolor path (${file}, got=${g})"
    fi

    index=$((index + 1))
    out_cli_env_bg="${ARTIFACT_LOCAL_DIR}/trns-keycolor-bg-env-cli-${file}.six"
    (
        unset SIXEL_BGCOLOR
        run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                      --env SIXEL_BGCOLOR=white \
                      -Llibpng:cms=0! -d fs -y raster \
                      "${input_png}" >"${out_cli_env_bg}"
    ) || {
        echo "not ok" "${index}" - "--env SIXEL_BGCOLOR render failed (${file})"
        continue
    }
    g="$(extract_leading_glyph <"${out_cli_env_bg}")"
    if [ "${g}" = "~" ]; then
        echo "ok" "${index}" - "--env SIXEL_BGCOLOR disables keycolor path (${file})"
    else
        echo "not ok" "${index}" - "--env SIXEL_BGCOLOR did not disable keycolor path (${file}, got=${g})"
    fi

    index=$((index + 1))
    out_proc_bg="${ARTIFACT_LOCAL_DIR}/trns-keycolor-bg-env-proc-${file}.six"
    SIXEL_BGCOLOR=white \
        run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                      -Llibpng:cms=0! -d fs -y raster \
                      "${input_png}" >"${out_proc_bg}" || {
        echo "not ok" "${index}" - "process SIXEL_BGCOLOR render failed (${file})"
        continue
    }
    g="$(extract_leading_glyph <"${out_proc_bg}")"
    if [ "${g}" = "~" ]; then
        echo "ok" "${index}" - "process SIXEL_BGCOLOR disables keycolor path (${file})"
    else
        echo "not ok" "${index}" - "process SIXEL_BGCOLOR did not disable keycolor path (${file}, got=${g})"
    fi
done

exit 0
