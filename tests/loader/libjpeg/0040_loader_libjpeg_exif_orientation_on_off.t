#!/bin/sh
# TAP test verifying libjpeg applies EXIF orientation when enabled.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}


echo "1..1"
set -v

input_exif="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_12x8.jpg"
input_plain="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_12x8.jpg"

exif_on=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibjpeg:orientation=on! "${input_exif}" 2>/dev/null) || {
    echo "not ok" 1 - "libjpeg orientation=on decode failed"
    exit 0
}

exif_off=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibjpeg:orientation=off! "${input_exif}" 2>/dev/null) || {
    echo "not ok" 1 - "libjpeg orientation=off decode failed"
    exit 0
}

test "${exif_on}" != "${exif_off}" || {
    echo "not ok" 1 - "libjpeg EXIF orientation on/off outputs were identical"
    exit 0
}

exif_default=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibjpeg! "${input_exif}" 2>/dev/null) || {
    echo "not ok" 1 - "libjpeg default orientation decode failed"
    exit 0
}

test "${exif_default}" = "${exif_on}" || {
    echo "not ok" 1 - "libjpeg default orientation did not match ON output"
    exit 0
}

plain_on=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibjpeg:orientation=on! "${input_plain}" 2>/dev/null) || {
    echo "not ok" 1 - "libjpeg plain orientation=on decode failed"
    exit 0
}

plain_off=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibjpeg:orientation=off! "${input_plain}" 2>/dev/null) || {
    echo "not ok" 1 - "libjpeg plain orientation=off decode failed"
    exit 0
}

test "${plain_on}" = "${plain_off}" || {
    echo "not ok" 1 - "libjpeg applied orientation despite missing EXIF metadata"
    exit 0
}

echo "ok" 1 - "libjpeg EXIF orientation on/off behavior is correct"
exit 0
