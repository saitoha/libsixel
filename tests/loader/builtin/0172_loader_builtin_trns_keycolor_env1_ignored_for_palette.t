#!/bin/sh
# Verify env=1 is ignored for palette+tRNS in builtin loader path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn3p08.png"
out_default="${ARTIFACT_LOCAL_DIR}/builtin_trns_keycolor_palette_default_tbbn3p08.six"
out_env1="${ARTIFACT_LOCAL_DIR}/builtin_trns_keycolor_palette_env1_tbbn3p08.six"

run_img2sixel --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - builtin palette+tRNS default render failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_env1}" || {
    echo "not ok 1 - builtin palette+tRNS env=1 render failed"
    exit 0
}

if cmp -s "${out_default}" "${out_env1}"; then
    echo "ok 1 - builtin env=1 is ignored for palette+tRNS output"
else
    echo "not ok 1 - builtin env=1 unexpectedly changes palette+tRNS output"
fi

exit 0
