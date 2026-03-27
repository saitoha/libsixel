#!/bin/sh
# Verify multi-input keycolor headers differ between opt-in and opt-out.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_a="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
input_b="${TOP_SRCDIR}/images/pngsuite/transparency/tbrn2c08.png"
out_on="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-multi-on-ab.six"
out_off="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-multi-off-ab.six"
keycolor_header="$(printf '\033P0;1q')"
out_on_payload=''
out_off_payload=''
out_line=''
out_on_has_header=0
out_off_has_header=0

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! -d fs -y raster \
              "${input_a}" "${input_b}" >"${out_on}" || {
    echo "not ok 1 - builtin multi-input opt-in render failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! -d fs -y raster \
              "${input_a}" "${input_b}" >"${out_off}" || {
    echo "not ok 1 - builtin multi-input opt-out render failed"
    exit 0
}

while IFS= read -r out_line || test -n "${out_line}"; do
    out_on_payload="${out_on_payload}${out_line}
"
done < "${out_on}"

while IFS= read -r out_line || test -n "${out_line}"; do
    out_off_payload="${out_off_payload}${out_line}
"
done < "${out_off}"

case "${out_on_payload}" in
    *"${keycolor_header}"*)
        out_on_has_header=1
        ;;
esac

case "${out_off_payload}" in
    *"${keycolor_header}"*)
        out_off_has_header=1
        ;;
esac

test "${out_on_has_header}" = 1 && test "${out_off_has_header}" = 0 || {
    echo "not ok 1 - builtin multi-input keycolor header gating mismatch"
    exit 0
}

echo "ok 1 - builtin multi-input keycolor header appears only with opt-in"

exit 0
