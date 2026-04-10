#!/bin/sh
# TAP test: PNG signature-only data fails in gd then falls through to builtin.

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

trace_log=$(set +xv; printf '\211PNG\r\n\032\n' | \
    SIXEL_LOADER_TRACE=1 ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -v -L gd,builtin! -ldisable - \
    2>&1 >/dev/null) && {
    echo "not ok" 1 - "gd,builtin unexpectedly accepted PNG signature-only data"
    exit 0
}

test "${trace_log#*libsixel: trying gd loader*}" != "${trace_log}" || {
    echo "not ok" 1 - "gd loader was not attempted for PNG signature-only"
    exit 0
}

test "${trace_log#*loader gd failed (GD error)*}" != "${trace_log}" || {
    echo "not ok" 1 - "gd failure did not report GD error"
    printf '%s\n' "${trace_log}" >&2
    exit 0
}

test "${trace_log#*libsixel: trying builtin loader*}" != "${trace_log}" || {
    echo "not ok" 1 - "builtin loader was not attempted after GD error"
    printf '%s\n' "${trace_log}" >&2
    exit 0
}

test "${trace_log#*loader builtin failed*}" != "${trace_log}" || {
    echo "not ok" 1 - "builtin loader failure was not observed"
    printf '%s\n' "${trace_log}" >&2
    exit 0
}

echo "ok" 1 - "PNG signature-only failure falls through gd then builtin"
exit 0
