#!/bin/sh
# TAP test ensuring issue #200 heap overflow is avoided.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



issue200="${top_srcdir}/tests/security/issue/data/200/POC_img2sixel_heap_buffer_overflow"

printf '1..1\n'
set -v

run_img2sixel --7bit-mode -8 --invert --palette-type=auto --verbose         "${issue200}" -o /dev/null || {
    fail 1 "heap overflow regression triggered"
    exit 0
}

pass 1 "heap overflow regression avoided"

exit 0
