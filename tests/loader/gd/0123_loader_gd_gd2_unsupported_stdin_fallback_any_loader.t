#!/bin/sh
# TAP test for unsupported GD GD2 stdin path delegating to another loader.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

test "${HAVE_DECL_GDIMAGECREATEFROMGD2PTR-}" != 1 || {
    printf "1..0 # SKIP GD GD2 decode support is available\n"
    exit 0
}

echo "1..1"
set -v

input_gd2="${TOP_SRCDIR}/tests/data/inputs/formats/sample-gd2-conv_test.gd2"
fallback_loader=""

test "${fallback_loader}" = "" && \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -ldisable - \
        < "${input_gd2}" >/dev/null 2>/dev/null && fallback_loader="builtin"
test "${fallback_loader}" = "" && test "${HAVE_GDK_PIXBUF2-}" = 1 && \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gdk-pixbuf2! -ldisable - \
        < "${input_gd2}" >/dev/null 2>/dev/null && fallback_loader="gdk-pixbuf2"
test "${fallback_loader}" = "" && test "${HAVE_WIC-}" = 1 && \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L wic! -ldisable - \
        < "${input_gd2}" >/dev/null 2>/dev/null && fallback_loader="wic"

test "${fallback_loader}" != "" || {
    printf "ok 1 # SKIP no fallback loader decoded GD2 stdin in this runtime\n"
    exit 0
}

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L gd! -ldisable - \
    < "${input_gd2}" 2>&1 >/dev/null) && {
    echo "not ok 1 - gd unexpectedly accepted unsupported GD2 stdin"
    exit 0
}

test "${msg#*runtime error: unable to decode input with available loaders*}" \
    != "${msg}" || {
    echo "not ok 1 - gd-only GD2 stdin failure missed generic decode error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*GD error*}" = "${msg}" || {
    echo "not ok 1 - gd-only GD2 stdin failure should not report GD error"
    printf '%s\n' "${msg}" >&2
    exit 0
}

trace_log=$(set +xv; SIXEL_LOADER_TRACE=1 ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -v -L "gd,${fallback_loader}!" -ldisable - \
    < "${input_gd2}" 2>&1 >/dev/null) || {
    echo "not ok 1 - gd stdin fallback failed for unsupported GD2"
    exit 0
}

test "${trace_log#*libsixel: trying gd loader*}" != "${trace_log}" || {
    echo "not ok 1 - gd loader was not attempted"
    exit 0
}

test "${trace_log#*libsixel: trying "${fallback_loader}" loader*}" \
    != "${trace_log}" || {
    echo "not ok 1 - ${fallback_loader} fallback was not attempted"
    exit 0
}

echo "ok 1 - unsupported GD GD2 stdin path delegates to ${fallback_loader}"
exit 0
