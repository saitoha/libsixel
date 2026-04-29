#!/bin/sh
# TAP test: PNG signature-only data fails in gd then reaches builtin.

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
    SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -L gd,builtin! -ldisable - \
    2>&1 >/dev/null) && {
    echo "not ok" 1 - "gd,builtin unexpectedly accepted PNG signature-only data"
    exit 0
}

after_gd=${trace_log#*LSXLOAD1|event=fail|loader=gd|code=L_ERR_GD*}
test "${after_gd}" != "${trace_log}" || {
    echo "not ok" 1 - "gd failure code was not reported"
    exit 0
}

after_builtin=${after_gd#*LSXLOAD1|event=try|loader=builtin|code=L_TRY*}
test "${after_builtin}" != "${after_gd}" || {
    echo "not ok" 1 - "builtin loader was not attempted after gd failure"
    exit 0
}

test "${after_builtin#*LSXLOAD1|event=fail|loader=builtin|code=*}" \
    != "${after_builtin}" || {
    echo "not ok" 1 - "builtin loader failure code was not reported"
    exit 0
}

echo "ok" 1 - "PNG signature-only failure reaches builtin after gd"
exit 0
