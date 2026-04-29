#!/bin/sh
# TAP test: stdin TGA gd failure still allows fallback chain progress.

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

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -ldisable - \
    < "${input_tga}" >/dev/null || {
    printf "ok 1 # SKIP builtin loader cannot decode stdin TGA\n"
    exit 0
}

trace_log=$(set +xv; head -c 32 "${input_tga}" | \
    SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L gd,builtin! -ldisable - 2>&1 >/dev/null) && {
    printf "ok 1 # SKIP truncated stdin TGA was accepted in this runtime\n"
    exit 0
}

after_gd=${trace_log#*LSXLOAD1|event=fail|loader=gd|code=L_ERR_GD*}
test "${after_gd}" != "${trace_log}" || {
    echo "not ok 1 - gd failure code was not reported"
    exit 0
}

test "${after_gd#*LSXLOAD1|event=try|loader=builtin|code=L_TRY*}" \
    != "${after_gd}" || {
    echo "not ok 1 - builtin loader was not attempted after gd failure"
    exit 0
}

echo "ok 1 - truncated stdin TGA progressed from gd to builtin"
exit 0
