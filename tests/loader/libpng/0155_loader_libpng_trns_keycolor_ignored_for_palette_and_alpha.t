#!/bin/sh
# Verify tRNS keycolor opt-in is ignored for palette and alpha-channel PNGs.

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

echo "1..11"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

index=0

palette_trns_files="tbbn3p08.png tbgn3p08.png tbwn3p08.png tbyn3p08.png tp0n3p08.png tp1n3p08.png tm3n3p02.png"
for file in ${palette_trns_files}; do
    index=$((index + 1))
    input_png="${TOP_SRCDIR}/images/pngsuite/transparency/${file}"
    default_out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-ignored-palette-default-${file}.six"
    optin_out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-ignored-palette-optin-${file}.six"

    run_img2sixel --env SIXEL_THREADS=4 \
                  -Llibpng:cms=0! \
                  -d fs -y raster \
                  "${input_png}" >"${default_out}" || {
        echo "not ok" "${index}" - "palette default render failed (${file})"
        continue
    }

    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  --env SIXEL_THREADS=4 \
                  -Llibpng:cms=0! \
                  -d fs -y raster \
                  "${input_png}" >"${optin_out}" || {
        echo "not ok" "${index}" - "palette opt-in render failed (${file})"
        continue
    }

    if cmp -s "${default_out}" "${optin_out}"; then
        echo "ok" "${index}" - "opt-in ignored for palette+tRNS (${file})"
    else
        echo "not ok" "${index}" - "opt-in unexpectedly changed palette+tRNS output (${file})"
    fi
done

alpha_chunk_files="basn4a08.png basn4a16.png basn6a08.png basn6a16.png"
for file in ${alpha_chunk_files}; do
    index=$((index + 1))
    input_png="${TOP_SRCDIR}/images/pngsuite/basic/${file}"
    default_out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-ignored-alpha-default-${file}.six"
    optin_out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-ignored-alpha-optin-${file}.six"

    run_img2sixel --env SIXEL_THREADS=4 \
                  -Llibpng:cms=0! \
                  -d fs -y raster \
                  "${input_png}" >"${default_out}" || {
        echo "not ok" "${index}" - "alpha default render failed (${file})"
        continue
    }

    run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  --env SIXEL_THREADS=4 \
                  -Llibpng:cms=0! \
                  -d fs -y raster \
                  "${input_png}" >"${optin_out}" || {
        echo "not ok" "${index}" - "alpha opt-in render failed (${file})"
        continue
    }

    if cmp -s "${default_out}" "${optin_out}"; then
        echo "ok" "${index}" - "opt-in ignored for alpha-chunk PNG (${file})"
    else
        echo "not ok" "${index}" - "opt-in unexpectedly changed alpha-chunk output (${file})"
    fi
done

exit 0
