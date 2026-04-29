#!/bin/sh
# TAP test for TGA chain: gd failure reaches builtin.

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
    SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -L gd,builtin! -ldisable - \
    2>&1 >/dev/null) || {
    echo "not ok 1 - gd,builtin chain failed for truncated TGA"
    exit 0
}

after_gd=${trace_log#*LSXLOAD1|event=fail|loader=gd|code=L_ERR_GD*}
test "${after_gd}" != "${trace_log}" || {
    echo "not ok 1 - gd failure code was not reported"
    exit 0
}

test "${after_gd#*LSXLOAD1|event=ok|loader=builtin|code=L_OK*}" \
    != "${after_gd}" || {
    echo "not ok 1 - builtin success code was not reported after gd"
    exit 0
}

echo "ok 1 - truncated TGA reaches builtin after gd failure"
exit 0
