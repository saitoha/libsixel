#!/bin/sh
# TAP test: gd delegates interlaced PNG to builtin fallback.

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

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-png-adam7-rgb.png" \
    >/dev/null && {
    echo "not ok" 1 - "gd unexpectedly accepted interlaced png"
    exit 0
}

trace_log=$(set +xv; SIXEL_LOADER_TRACE=1 ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -v \
    -L gd,builtin! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-png-adam7-rgb.png" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "gd,builtin fallback failed for interlaced png"
    exit 0
}

test "${trace_log#*libsixel: trying builtin loader*}" != "${trace_log}" || {
    echo "not ok" 1 - "builtin loader was not attempted for interlaced png"
    exit 0
}

test "${trace_log#*libsixel: trying gd loader*}" = "${trace_log}" || {
    echo "not ok" 1 - "gd loader should be skipped for interlaced png"
    exit 0
}

echo "ok" 1 - "gd delegates interlaced png and builtin fallback succeeds"
exit 0
