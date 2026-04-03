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

input_exif="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_12x8.png"
input_plain="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_12x8.png"

static_on=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lcoregraphics:orientation=on! "${input_exif}" 2>/dev/null) || {
    echo "not ok 1 - coregraphics static orientation=on decode failed"
    exit 0
}

static_off=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lcoregraphics:orientation=off! "${input_exif}" 2>/dev/null) || {
    echo "not ok 1 - coregraphics static orientation=off decode failed"
    exit 0
}

test "${static_on}" != "${static_off}" || {
    echo "not ok 1 - coregraphics static EXIF orientation on/off were equal"
    exit 0
}

static_default=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lcoregraphics! "${input_exif}" 2>/dev/null) || {
    echo "not ok 1 - coregraphics static default orientation decode failed"
    exit 0
}

test "${static_default}" = "${static_on}" || {
    echo "not ok 1 - coregraphics static default output did not match ON"
    exit 0
}

plain_on=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lcoregraphics:orientation=on! "${input_plain}" 2>/dev/null) || {
    echo "not ok 1 - coregraphics plain static orientation=on decode failed"
    exit 0
}

plain_off=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lcoregraphics:orientation=off! "${input_plain}" 2>/dev/null) || {
    echo "not ok 1 - coregraphics plain static orientation=off decode failed"
    exit 0
}

test "${plain_on}" = "${plain_off}" || {
    echo "not ok 1 - coregraphics static output changed without EXIF"
    exit 0
}

echo "ok 1 - coregraphics static EXIF orientation on/off works"
exit 0
