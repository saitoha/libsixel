#!/bin/sh
# Verify scale geometry promotes PAL8+transparent frames to RGB+mask path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/pal8-trns-key0.png"
scale_log=""
scale_out=""
keycolor_header="$(printf '\033P0;1q')"
scale_rgb=0
scale_has_keycolor=0

scale_log=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -w7 -h3 -r nearest "${input_png}" 2>&1 >/dev/null) || {
    echo "not ok 1 - scaled pal8 transparent render failed"
    exit 0
}

scale_out=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -w7 -h3 -r nearest "${input_png}") || {
    echo "not ok 1 - scaled pal8 transparent sixel output failed"
    exit 0
}

case "${scale_log}" in
    *"formats: source=rgb888 work=rgb888"*)
        scale_rgb=1
        ;;
esac

case "${scale_out}" in
    *"${keycolor_header}"*)
        scale_has_keycolor=1
        ;;
esac

test "${scale_rgb}" = 1 || {
    echo "not ok 1 - scale path did not promote to rgb888 work format"
    exit 0
}

test "${scale_has_keycolor}" = 1 || {
    echo "not ok 1 - scaled output lost transparent keycolor header"
    exit 0
}

echo "ok 1 - scale geometry promotes pal8 transparent input to rgb+mask path"
exit 0
