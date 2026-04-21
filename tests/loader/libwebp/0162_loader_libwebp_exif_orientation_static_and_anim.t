#!/bin/sh
# TAP test verifying libwebp EXIF orientation for static and animated inputs.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}


echo "1..1"
set -v

input_static_exif="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_12x8.webp"
input_static_plain="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_12x8.webp"
input_anim_exif="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_anim_12x8.webp"

static_on=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibwebp:orientation=on! "${input_static_exif}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libwebp static orientation=on decode failed"
    exit 0
}

static_off=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibwebp:orientation=off! "${input_static_exif}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libwebp static orientation=off decode failed"
    exit 0
}

test "${static_on}" != "${static_off}" || {
    echo "not ok" 1 - "libwebp static EXIF orientation on/off outputs were identical"
    exit 0
}

static_default=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibwebp! "${input_static_exif}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libwebp static default orientation decode failed"
    exit 0
}

test "${static_default}" = "${static_on}" || {
    echo "not ok" 1 - "libwebp static default orientation did not match ON output"
    exit 0
}

plain_static_on=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibwebp:orientation=on! "${input_static_plain}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libwebp plain static orientation=on decode failed"
    exit 0
}

plain_static_off=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibwebp:orientation=off! "${input_static_plain}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libwebp plain static orientation=off decode failed"
    exit 0
}

test "${plain_static_on}" = "${plain_static_off}" || {
    echo "not ok" 1 - "libwebp static output changed without EXIF metadata"
    exit 0
}

anim_on_f1=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibwebp:orientation=on! -S --start-frame=1 \
    "${input_anim_exif}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libwebp animation frame1 orientation=on decode failed"
    exit 0
}

anim_off_f1=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibwebp:orientation=off! -S --start-frame=1 \
    "${input_anim_exif}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libwebp animation frame1 orientation=off decode failed"
    exit 0
}

test "${anim_on_f1}" != "${anim_off_f1}" || {
    echo "not ok" 1 - "libwebp animation frame1 orientation on/off outputs were identical"
    exit 0
}

echo "ok" 1 - "libwebp EXIF orientation works for static and animation frames"
exit 0
