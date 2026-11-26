#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# A bidirectional LUT can serve as output but has no separable working model.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
msg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Wicc:${TOP_SRCDIR}/tests/data/color-numeric/relative-paired-lut.icc" \
    </dev/null 2>&1) && {
    printf '\n%s\n' 'not ok 1 - accepted LUT working profile'
    exit 0
}
case "$msg" in *'matrix/TRC RGB profile'*) true ;; *) false ;; esac || {
    printf '%s\n' "$msg" >&2
    printf '\n%s\n' 'not ok 1 - missing LUT working diagnostic'
    exit 0
}
printf '\n%s\n' 'ok 1 - LUT working profile rejected'
exit 0
