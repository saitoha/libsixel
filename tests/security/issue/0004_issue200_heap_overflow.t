#!/bin/sh
# TAP test ensuring issue #200 heap overflow is avoided.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}



issue200="${TOP_SRCDIR}/tests/data/security/issue/data/200/POC_img2sixel_heap_buffer_overflow"

printf '1..1\n'
set -v

run_img2sixel --7bit-mode -8 --invert --palette-type=auto --verbose         "${issue200}" -o /dev/null || {
    fail 1 "heap overflow regression triggered"
    exit 0
}

pass 1 "heap overflow regression avoided"

exit 0
