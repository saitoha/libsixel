#!/bin/sh
# Verify builtin loader rejects PSD (image resources section length overflow).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/corrupted/bad_image_resources_length.psd"

run_img2sixel -L builtin! "${input_psd}" >/dev/null && {
    echo "not ok" 1 - "image resources section length overflow was unexpectedly accepted"
    exit 0
}

echo "ok" 1 - "image resources section length overflow is rejected"
exit 0
