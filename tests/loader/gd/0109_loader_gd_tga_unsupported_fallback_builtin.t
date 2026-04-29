#!/bin/sh
# TAP test for unsupported GD TGA path delegating to builtin fallback.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMTGAPTR-}" != 1 || {
    printf "1..0 # SKIP GD TGA decode support is available\n"
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

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable \
    "${input_tga}" 2>&1 >/dev/null) && {
    echo "not ok 1 - gd unexpectedly accepted unsupported TGA"
    exit 0
}

test "${msg#*runtime error: unable to decode input with available loaders*}" \
    != "${msg}" || {
    echo "not ok 1 - gd-only TGA failure missed generic decode error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*GD error*}" = "${msg}" || {
    echo "not ok 1 - gd-only TGA failure should not report GD error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -L gd,builtin! -ldisable "${input_tga}" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - gd,builtin fallback failed for unsupported TGA"
    exit 0
}

test "${trace_log#*libsixel: trying gd loader*}" != "${trace_log}" || {
    echo "not ok 1 - gd loader was not attempted"
    exit 0
}

test "${trace_log#*libsixel: trying builtin loader*}" != "${trace_log}" || {
    echo "not ok 1 - builtin fallback was not attempted"
    exit 0
}

echo "ok 1 - unsupported GD TGA path delegates to builtin"
exit 0
