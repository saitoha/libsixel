#!/bin/sh
# TAP test for unsupported GD WebP stdin path delegating to libwebp fallback.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMWEBPPTR-}" != 1 || {
    printf "1..0 # SKIP GD WebP decode support is available\n"
    exit 0
}

echo "1..1"
set -v

input_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64.webp"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable - \
    <"${input_webp}" >/dev/null || {
    printf "ok 1 # SKIP libwebp loader cannot decode stdin WebP\n"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable - \
    <"${input_webp}" 2>&1 >/dev/null) && {
    echo "not ok 1 - gd unexpectedly accepted unsupported stdin WebP"
    exit 0
}

test "${msg#*runtime error: unable to decode input with available loaders*}" \
    != "${msg}" || {
    echo "not ok 1 - gd-only stdin WebP failure missed generic decode error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*GD error*}" = "${msg}" || {
    echo "not ok 1 - gd-only stdin WebP failure should not report GD error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

trace_log=$(set +xv; SIXEL_LOADER_TRACE=1 ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -v -L gd,libwebp! -ldisable - <"${input_webp}" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - gd,libwebp fallback failed for stdin WebP"
    exit 0
}

test "${trace_log#*libsixel: trying libwebp loader*}" != "${trace_log}" || {
    echo "not ok 1 - libwebp fallback was not attempted for stdin WebP"
    exit 0
}

echo "ok 1 - unsupported stdin WebP delegates from gd to libwebp"
exit 0
