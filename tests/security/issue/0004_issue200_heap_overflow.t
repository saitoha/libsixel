#!/bin/sh
# TAP test ensuring issue #200 heap overflow is avoided.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

printf '1..1\n'
set -v




issue200="${TOP_SRCDIR}/tests/data/security/issue/data/200/POC_img2sixel_heap_buffer_overflow"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --7bit-mode -8 --invert --palette-type=auto --verbose         "${issue200}" -o /dev/null || {
    echo "not ok" 1 - "heap overflow regression triggered"
    exit 0
}

echo "ok" 1 - "heap overflow regression avoided"

exit 0
