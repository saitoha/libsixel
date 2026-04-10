#!/bin/sh
# TAP test: forced gd reports GD error for truncated stdin WBMP stream.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMWBMPPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMWBMPPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_wbmp="${TOP_SRCDIR}/tests/data/inputs/formats/snake-wbmp-bilevel.wbmp"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable \
    "${input_wbmp}" >/dev/null || {
    printf "ok 1 # SKIP gd backend does not decode WBMP in this runtime\n"
    exit 0
}

msg=$(set +xv; head -c 32 "${input_wbmp}" | \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable - \
    2>&1 >/dev/null) && {
    echo "not ok 1 - forced gd loader accepted truncated stdin WBMP"
    exit 0
}

test "${msg#*GD error*}" != "${msg}" || {
    echo "not ok 1 - forced gd stdin WBMP failure missed GD error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*runtime error: unable to decode input with available loaders*}" \
    = "${msg}" || {
    echo "not ok 1 - forced gd stdin WBMP degraded to generic error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok 1 - forced gd stdin WBMP reports GD-specific failure"
exit 0
