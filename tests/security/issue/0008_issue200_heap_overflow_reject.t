#!/bin/sh
# TAP test for issue #200 regression using the reported CLI flags.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"


issue200="${TOP_SRCDIR}/tests/security/issue/data/200/POC_img2sixel_heap_buffer_overflow"

printf '1..1\n'
set -v

set +e
run_img2sixel --7bit-mode -8 --invert --palette-type=auto --verbose         "${issue200}" -o /dev/null
command_status=$?
set -e

# Accept success or mapped error exits (1/2/3) without crashing.
test "${command_status}" -le 3 || {
    fail 1 "reported heap overflow path failed"
    exit 0
}

pass 1 "reported heap overflow path rejected safely"

exit 0
