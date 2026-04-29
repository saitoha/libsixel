#!/bin/sh
# TAP test for unsupported GD TIFF path delegating to libtiff fallback.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMTIFFPTR-}" != 1 || {
    printf "1..0 # SKIP GD TIFF decode support is available\n"
    exit 0
}

echo "1..1"
set -v

input_tiff="${TOP_SRCDIR}/tests/data/inputs/formats/snake-tiff-zip-rgb.tiff"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable \
    "${input_tiff}" >/dev/null && {
    echo "not ok 1 - gd unexpectedly accepted unsupported TIFF"
    exit 0
}

trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -L gd,libtiff! -ldisable "${input_tiff}" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - gd,libtiff fallback failed for TIFF"
    exit 0
}

test "${trace_log#*libsixel: trying gd loader*}" != "${trace_log}" || {
    echo "not ok 1 - gd loader was not attempted"
    exit 0
}

test "${trace_log#*libsixel: trying libtiff loader*}" != "${trace_log}" || {
    echo "not ok 1 - libtiff fallback was not attempted"
    exit 0
}

echo "ok 1 - unsupported GD TIFF path delegates to libtiff"
exit 0
