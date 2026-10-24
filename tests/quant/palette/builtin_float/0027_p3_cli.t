#!/bin/sh
# Verify built-in P3 colors through CLI defaults and the builtin PPM loader.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo "1..1"
set -v

actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=greedy:global_budget=1 \
    -Lbuiltin:cms_engine=none! -bxterm16 -dnone -Udisplay-p3 - <<'PPM'
P3
2 1
255
255 0 0 0 255 0
PPM
) || {
    echo "not ok 1 - builtin palette P3 CLI failed"
    exit 0
}
case "${actual}" in *'#9;2;92;20;14'*'#10;2;46;99;30'*) true ;; *) false ;; esac || {
    echo "not ok 1 - builtin palette P3 primary definitions"
    exit 0
}
echo "ok 1 - builtin palette P3 CLI defines independent primary values"
exit 0
