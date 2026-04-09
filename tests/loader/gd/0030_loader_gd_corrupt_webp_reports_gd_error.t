#!/bin/sh
# TAP test: forced gd loader reports GD failure for malformed WebP stream.

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

msg=$(set +xv; head -c 64 "${TOP_SRCDIR}/tests/data/inputs/snake_64.webp" | \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable - \
    2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced gd loader accepted malformed WebP stream"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*GD error*}" != "${msg}" || {
    echo "not ok" 1 - "expected GD error diagnostic was missing"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*runtime error: unable to decode input with available loaders*}" \
    = "${msg}" || {
    echo "not ok" 1 - "unexpected generic loader-chain failure message"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "forced gd loader reports GD failure for malformed WebP"
exit 0
