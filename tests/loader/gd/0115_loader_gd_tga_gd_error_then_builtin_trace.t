#!/bin/sh
# TAP test for TGA chain: gd reports GD error then builtin is attempted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMTGAPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMTGAPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_tga="${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type2-rgb.tga"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -ldisable \
    "${input_tga}" >/dev/null || {
    printf "ok 1 # SKIP builtin loader cannot decode TGA in this runtime\n"
    exit 0
}

trace_log=$(set +xv; head -c 64 "${input_tga}" | \
    SIXEL_LOADER_TRACE=1 ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -v -L gd,builtin! -ldisable - \
    2>&1 >/dev/null) || {
    echo "not ok 1 - gd,builtin chain failed for truncated TGA"
    exit 0
}

test "${trace_log#*libsixel: trying gd loader*}" != "${trace_log}" || {
    echo "not ok 1 - gd loader was not attempted"
    exit 0
}

test "${trace_log#*loader gd failed (GD error)*}" != "${trace_log}" || {
    echo "not ok 1 - gd failure was not reported as GD error"
    printf '%s\n' "${trace_log}" >&2
    exit 0
}

test "${trace_log#*libsixel: trying builtin loader*}" != "${trace_log}" || {
    echo "not ok 1 - builtin fallback was not attempted"
    printf '%s\n' "${trace_log}" >&2
    exit 0
}

echo "ok 1 - truncated TGA shows gd GD-error then builtin fallback"
exit 0
