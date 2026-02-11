#!/bin/sh
# TAP test for issue #200 regression using the reported CLI flags.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



check_exit() {
    set +e
    run_img2sixel "$@"
    rc=$?
    set -e

    # Accept success or mapped error exits (1/2/3) without crashing.
    [ "${rc}" -le 3 ]
}

issue200="${top_srcdir}/tests/security/issue/data/200/POC_img2sixel_heap_buffer_overflow"

printf '1..1\n'
set -v

check_exit --7bit-mode -8 --invert --palette-type=auto --verbose         "${issue200}" -o /dev/null || {
    fail 1 "reported heap overflow path failed"
    exit 0
}

pass 1 "reported heap overflow path rejected safely"

exit 0
