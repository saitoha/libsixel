#!/bin/sh
# Verify builtin loader rejects PSD (dimension larger than STB max).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/corrupted/too_large_dimension.psd"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >/dev/null && {
    echo "not ok" 1 - "dimension larger than STB max was unexpectedly accepted"
    exit 0
}

echo "ok" 1 - "dimension larger than STB max is rejected"
exit 0
