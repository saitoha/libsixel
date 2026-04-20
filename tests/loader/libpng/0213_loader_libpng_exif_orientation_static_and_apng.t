#!/bin/sh
# TAP test verifying libpng EXIF orientation for static PNG and APNG frames.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_static_exif="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_12x8.png"
input_static_plain="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_12x8.png"
input_apng_exif="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_apng_12x8_rgba_loop2.png"
input_apng_plain="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_apng_12x8_rgba_loop2.png"

static_on=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=on! "${input_static_exif}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libpng static orientation=on decode failed"
    exit 0
}

static_off=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=off! "${input_static_exif}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libpng static orientation=off decode failed"
    exit 0
}

test "${static_on}" != "${static_off}" || {
    echo "not ok" 1 - "libpng static EXIF orientation on/off outputs were identical"
    exit 0
}

static_default=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng! "${input_static_exif}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libpng static default orientation decode failed"
    exit 0
}

test "${static_default}" = "${static_on}" || {
    echo "not ok" 1 - "libpng static default orientation did not match ON output"
    exit 0
}

plain_static_on=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=on! "${input_static_plain}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libpng plain static orientation=on decode failed"
    exit 0
}

plain_static_off=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=off! "${input_static_plain}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libpng plain static orientation=off decode failed"
    exit 0
}

test "${plain_static_on}" = "${plain_static_off}" || {
    echo "not ok" 1 - "libpng static output changed without EXIF metadata"
    exit 0
}

apng_on_f0=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=on! -S --start-frame=0 \
    "${input_apng_exif}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libpng APNG frame0 orientation=on decode failed"
    exit 0
}

apng_off_f0=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=off! -S --start-frame=0 \
    "${input_apng_exif}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libpng APNG frame0 orientation=off decode failed"
    exit 0
}

test "${apng_on_f0}" != "${apng_off_f0}" || {
    echo "not ok" 1 - "libpng APNG frame0 orientation on/off outputs were identical"
    exit 0
}

apng_on_f1=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=on! -S --start-frame=1 \
    "${input_apng_exif}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libpng APNG frame1 orientation=on decode failed"
    exit 0
}

apng_off_f1=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=off! -S --start-frame=1 \
    "${input_apng_exif}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libpng APNG frame1 orientation=off decode failed"
    exit 0
}

test "${apng_on_f1}" != "${apng_off_f1}" || {
    echo "not ok" 1 - "libpng APNG frame1 orientation on/off outputs were identical"
    exit 0
}

plain_apng_on=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=on! -S --start-frame=1 \
    "${input_apng_plain}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libpng plain APNG orientation=on decode failed"
    exit 0
}

plain_apng_off=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:orientation=off! -S --start-frame=1 \
    "${input_apng_plain}" -p 2 2>/dev/null) || {
    echo "not ok" 1 - "libpng plain APNG orientation=off decode failed"
    exit 0
}

test "${plain_apng_on}" = "${plain_apng_off}" || {
    echo "not ok" 1 - "libpng APNG output changed without EXIF metadata"
    exit 0
}

echo "ok" 1 - "libpng EXIF orientation works for static PNG and APNG frames"
exit 0
