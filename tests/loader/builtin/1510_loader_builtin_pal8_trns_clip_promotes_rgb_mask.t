#!/bin/sh
# Verify clip geometry promotes PAL8+transparent frames to RGB+mask path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/pal8-trns-key0.png"
base_log=""
clip_log=""
clip_out=""
keycolor_header="$(printf '\033P0;1q')"
base_pal8=0
clip_rgb=0
clip_has_keycolor=0

base_log=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -v -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    "${input_png}" 2>&1 >/dev/null) || {
    echo "not ok 1 - baseline pal8 transparent render failed"
    exit 0
}

clip_log=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -v -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -c2x1+0+0 "${input_png}" 2>&1 >/dev/null) || {
    echo "not ok 1 - clipped pal8 transparent render failed"
    exit 0
}

clip_out=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -c2x1+0+0 "${input_png}") || {
    echo "not ok 1 - clipped pal8 transparent sixel output failed"
    exit 0
}

case "${base_log}" in
    *"formats: source=pal8 work=pal8"*)
        base_pal8=1
        ;;
esac

case "${clip_log}" in
    *"formats: source=rgb888 work=rgb888"*)
        clip_rgb=1
        ;;
esac

case "${clip_out}" in
    *"${keycolor_header}"*)
        clip_has_keycolor=1
        ;;
esac

test "${base_pal8}" = 1 || {
    echo "not ok 1 - baseline path did not stay pal8"
    exit 0
}

test "${clip_rgb}" = 1 || {
    echo "not ok 1 - clip path did not promote to rgb888 work format"
    exit 0
}

test "${clip_has_keycolor}" = 1 || {
    echo "not ok 1 - clipped output lost transparent keycolor header"
    exit 0
}

echo "ok 1 - clip geometry promotes pal8 transparent input to rgb+mask path"
exit 0
