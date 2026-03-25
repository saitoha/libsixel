#!/bin/sh
# Verify high-color mode never emits keycolor header.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
out_on="${ARTIFACT_LOCAL_DIR}/trns-keycolor-highcolor-on-tbbn0g04.six"
out_off="${ARTIFACT_LOCAL_DIR}/trns-keycolor-highcolor-off-tbbn0g04.six"
keycolor_header="$(printf '\033P0;1q')"
out_on_payload=''
out_off_payload=''
out_line=''
out_on_has_header=0
out_off_has_header=0

run_img2sixel -I \
              --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -Llibpng:cms_engine=none! \
              "${input_png}" >"${out_on}" || {
    echo "not ok 1 - highcolor opt-in render failed"
    exit 0
}

run_img2sixel -I \
              --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Llibpng:cms_engine=none! \
              "${input_png}" >"${out_off}" || {
    echo "not ok 1 - highcolor opt-out render failed"
    exit 0
}

while IFS= read -r out_line || [ -n "${out_line}" ]; do
    out_on_payload="${out_on_payload}${out_line}
"
done < "${out_on}"

while IFS= read -r out_line || [ -n "${out_line}" ]; do
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

if [ "${out_on_has_header}" = 0 ] && [ "${out_off_has_header}" = 0 ]; then
    echo "ok 1 - highcolor mode ignores keycolor header"
else
    echo "not ok 1 - highcolor mode emitted keycolor header"
fi

exit 0
