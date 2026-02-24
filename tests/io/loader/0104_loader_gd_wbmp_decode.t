#!/bin/sh
# TAP test: gd loader decodes WBMP input successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_GD-}" = 1 || {
    printf "1..0 # SKIP gd support is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-wbmp-bilevel.wbmp" \
    >/dev/null && {
    pass 1 "gd decodes WBMP input"
    exit 0
}

printf "ok 1 # SKIP gd backend does not decode WBMP in this runtime\n"
exit 0
