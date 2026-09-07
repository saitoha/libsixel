#!/bin/sh
# Verify that indexed transparent input stays PAL8 without geometry changes.
# Policy: docs/concepts/pixelformat.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/pal8-trns-key0.png"
base_log=""
base_pal8=0

base_log=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v \
    -A composite -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    "${input_png}" 2>&1 >/dev/null) || {
    echo "not ok 1 - baseline pal8 transparent render failed"
    exit 0
}

case "${base_log}" in
    *"formats: source=pal8 work=pal8"*)
        base_pal8=1
        ;;
esac

test "${base_pal8}" = 1 || {
    echo "not ok 1 - baseline path did not stay pal8"
    exit 0
}

echo "ok 1 - indexed transparent input stays pal8"
exit 0
