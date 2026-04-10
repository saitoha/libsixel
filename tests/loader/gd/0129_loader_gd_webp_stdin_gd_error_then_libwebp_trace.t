#!/bin/sh
# TAP test: stdin WebP GD error still allows fallback chain progress.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMWEBPPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMWEBPPTR is unavailable\n"
    exit 0
}

test "${HAVE_LIBWEBPDECODER-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64.webp"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable - \
    < "${input_webp}" >/dev/null || {
    printf "ok 1 # SKIP libwebp loader cannot decode stdin WebP\n"
    exit 0
}

trace_log=$(set +xv; head -c 32 "${input_webp}" | \
    SIXEL_LOADER_TRACE=1 ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v \
    -L gd,libwebp! -ldisable - 2>&1 >/dev/null) && {
    printf "ok 1 # SKIP truncated stdin WebP was accepted in this runtime\n"
    exit 0
}

test "${trace_log#*libsixel: trying gd loader*}" != "${trace_log}" || {
    echo "not ok 1 - gd loader was not attempted for truncated stdin WebP"
    exit 0
}

test "${trace_log#*libsixel: trying libwebp loader*}" != "${trace_log}" || {
    echo "not ok 1 - libwebp loader was not attempted after gd error"
    exit 0
}

echo "ok 1 - truncated stdin WebP progressed from gd to libwebp"
exit 0
