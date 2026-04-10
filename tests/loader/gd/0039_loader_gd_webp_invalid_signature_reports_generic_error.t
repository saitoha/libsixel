#!/bin/sh
# TAP test: forced gd loader reports generic error for non-WebP signature.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMWEBPPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMWEBPPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.webp" >/dev/null || {
    printf "ok 1 # SKIP gd backend does not decode WebP in this runtime\n"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/corrupted/bad_riff_webp_signature.webp" \
    2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced gd loader unexpectedly accepted invalid WebP"
    exit 0
}

test "${msg#*runtime error: unable to decode input with available loaders*}" \
    != "${msg}" || {
    echo "not ok" 1 - "forced gd invalid WebP missed generic decode error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*GD error*}" = "${msg}" || {
    echo "not ok" 1 - "forced gd invalid WebP should not report GD error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "forced gd invalid WebP reports delegated generic error"
exit 0
