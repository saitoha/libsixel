#!/bin/sh
# Verify builtin APNG default output contains the keycolor DCS header.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
out_default="${ARTIFACT_LOCAL_DIR}/builtin-apng-trns-keycolor-default-header.six"
keycolor_header="$(printf '\033P0;1q')"
out_payload=''
has_header=0

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! \
              -d fs:scan=raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - builtin APNG default render failed"
    exit 0
}

set +x
IFS= read -r out_payload < "${out_default}" || test -n "${out_payload}"

case "${out_payload}" in
    *"${keycolor_header}"*)
        has_header=1
        ;;
esac

test "${has_header}" = 1 || {
    echo "not ok 1 - builtin APNG default output lost keycolor DCS header"
    exit 0
}

echo "ok 1 - builtin APNG default output keeps keycolor DCS header"

exit 0
