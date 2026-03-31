#!/bin/sh
test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

set -eux

input_pic="${TOP_SRCDIR}/tests/data/inputs/formats/pic_valid_raw_rgb_2x2.pic"

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! - <"${input_pic}" >/dev/null || {
    echo "not ok 1 - loader builtin pic decode from stdin"
    exit 0
}

echo "ok 1 - loader builtin pic decode from stdin"
exit 0
