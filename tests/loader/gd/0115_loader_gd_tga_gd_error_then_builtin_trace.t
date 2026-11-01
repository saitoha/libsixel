#!/bin/sh
# A valid paletted TGA recovers through builtin after a GD decode error.

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

trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -L gd,builtin! -ldisable - \
    < "${TOP_SRCDIR}/tests/data/inputs/formats/tga-pal-rgba-2x2-top-left.tga" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - gd,builtin chain failed for valid paletted TGA"
    exit 0
}

# A future GD implementation may decode this format itself. That does not
# exercise fallback and must not be reported as a passing fallback test.
case "${trace_log}" in *"LSXLOAD1|event=ok|loader=gd|code=L_OK"*) false ;; *) true ;; esac || {
    echo "ok 1 # SKIP GD supports paletted TGA without fallback"
    exit 0
}

case "${trace_log}" in *"LSXLOAD1|event=fail|loader=gd|code=L_ERR_GD"*"LSXLOAD1|event=try|loader=builtin|code=L_TRY"*"LSXLOAD1|event=ok|loader=builtin|code=L_OK"*) true ;; *) false ;; esac || {
    echo "not ok 1 - builtin success code was not reported after gd"
    exit 0
}

echo "ok 1 - valid paletted TGA succeeds through builtin after GD error"
exit 0
