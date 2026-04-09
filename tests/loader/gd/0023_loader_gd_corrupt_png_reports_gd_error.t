#!/bin/sh
# TAP test: forced gd loader reports GD failure for malformed PNG stream.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

msg=$(set +xv; head -c 64 \
    "${TOP_SRCDIR}/tests/data/inputs/formats/rgb.png" | \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable - \
    2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced gd loader accepted malformed PNG stream"
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
    echo "not ok" 1 - "forced gd malformed PNG fell back to generic error"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "forced gd malformed PNG reports GD-specific failure"
exit 0
