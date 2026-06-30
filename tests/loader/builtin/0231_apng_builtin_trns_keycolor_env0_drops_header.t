#!/bin/sh
# Verify builtin APNG output drops keycolor DCS header when SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
out_off="${ARTIFACT_LOCAL_DIR}/builtin-apng-trns-keycolor-env0-header.six"
keycolor_header="$(printf '\033P0;0q')"
out_payload=''
has_header=0

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Lbuiltin! \
              -d fs:scan=raster \
              "${input_png}" >"${out_off}" || {
    echo "not ok 1 - builtin APNG render failed with SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0"
    exit 0
}

set +x
IFS= read -r out_payload < "${out_off}" || test -n "${out_payload}"

case "${out_payload}" in
    *"${keycolor_header}"*)
        has_header=1
        ;;
esac

test "${has_header}" = 1 && {
    echo "not ok 1 - builtin APNG kept keycolor DCS header with SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0"
    exit 0
}

echo "ok 1 - builtin APNG drops keycolor DCS header with SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0"
exit 0
