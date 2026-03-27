#!/bin/sh
# Verify reqcolors=255 moves off pal8 work format in builtin path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-multi0-semi-icc.png"
out_lo="${ARTIFACT_LOCAL_DIR}/builtin-req255-cms0.six"
log_lo="${ARTIFACT_LOCAL_DIR}/builtin-req255-cms0.log"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Lbuiltin:cms_engine=none! \
              -B#ffffff -d none -p255 \
              "${input_png}" >"${out_lo}" 2>"${log_lo}" || {
    echo "not ok 1 - builtin reqcolors=255 render failed"
    exit 0
}

grep -q 'formats: source=pal8 work=pal8' "${log_lo}" && {
    echo "not ok 1 - builtin reqcolors=255 unexpectedly kept pal8 work format"
    exit 0
}

echo "ok 1 - builtin reqcolors=255 moved off pal8 work format"

exit 0
