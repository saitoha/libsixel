#!/bin/sh
# TAP test confirming builtin loader keeps indexed TGA palette path.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

input_tga="${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type1-pal8.tga"

run_img2sixel -L builtin! "${input_tga}" >/dev/null || {
    fail 1 "builtin loader indexed TGA palette path failed"
    exit 0
}

pass 1 "builtin loader keeps indexed TGA palette path"
exit 0
