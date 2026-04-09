#!/bin/sh
# TAP test: gd rejects GIF directly and fallback decodes interlaced GIF.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable in this build\n";
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-gif-interlaced.gif" \
    >/dev/null && {
    echo "not ok" 1 - "gd unexpectedly accepted interlaced GIF"
    exit 0
}

trace_log=$(set +xv; SIXEL_LOADER_TRACE=1 ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -v \
    -L gd,builtin! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-gif-interlaced.gif" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "fallback failed to decode interlaced GIF"
    exit 0
}

test "${trace_log#*libsixel: trying builtin loader*}" != "${trace_log}" || {
    echo "not ok" 1 - "builtin loader was not attempted for interlaced GIF"
    exit 0
}

test "${trace_log#*libsixel: trying gd loader*}" = "${trace_log}" || {
    echo "not ok" 1 - "gd loader should be skipped for interlaced GIF"
    exit 0
}

echo "ok" 1 - "fallback decodes interlaced GIF"
exit 0
