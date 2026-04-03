#!/bin/sh
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_exif="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_apng_12x8_rgba_loop2.png"
input_plain="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_apng_12x8_rgba_loop2.png"

apng_on=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lcoregraphics:orientation=on! -S --start-frame=1 \
    "${input_exif}" 2>/dev/null) || {
    echo "not ok 1 - coregraphics APNG frame1 orientation=on decode failed"
    exit 0
}

apng_off=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lcoregraphics:orientation=off! -S --start-frame=1 \
    "${input_exif}" 2>/dev/null) || {
    echo "not ok 1 - coregraphics APNG frame1 orientation=off decode failed"
    exit 0
}

test "${apng_on}" != "${apng_off}" || {
    echo "not ok 1 - coregraphics APNG frame1 orientation on/off were equal"
    exit 0
}

apng_default=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lcoregraphics! -S --start-frame=1 "${input_exif}" 2>/dev/null) || {
    echo "not ok 1 - coregraphics APNG frame1 default orientation failed"
    exit 0
}

test "${apng_default}" = "${apng_on}" || {
    echo "not ok 1 - coregraphics APNG default output did not match ON"
    exit 0
}

plain_on=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lcoregraphics:orientation=on! -S --start-frame=1 \
    "${input_plain}" 2>/dev/null) || {
    echo "not ok 1 - coregraphics plain APNG orientation=on decode failed"
    exit 0
}

plain_off=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lcoregraphics:orientation=off! -S --start-frame=1 \
    "${input_plain}" 2>/dev/null) || {
    echo "not ok 1 - coregraphics plain APNG orientation=off decode failed"
    exit 0
}

test "${plain_on}" = "${plain_off}" || {
    echo "not ok 1 - coregraphics APNG output changed without EXIF"
    exit 0
}

echo "ok 1 - coregraphics APNG orientation works with start-frame"
exit 0
